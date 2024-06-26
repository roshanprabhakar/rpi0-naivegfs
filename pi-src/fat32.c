#include "rpi.h"
#include "fat32.h"
//#include <string.h>

// Reads (sector lba, buffer dst, number of sectors)
static int (*readsector)(unsigned lba, unsigned char *buf, unsigned num);
static int (*writesector)(const unsigned char *buf, unsigned int lba, unsigned int num);

void read_cluster(uint32_t, uint8_t *);
void write_cluster(uint32_t, uint8_t *);
uint32_t cluster_no_to_lba(uint32_t);

static int did_init = 0;
static uint8_t partitions_in_use = 0;
static int chosen_partition;

static struct mbr boot_sector;
static struct volume_id all_vol_ids[4];
static struct fat_volume_data vol_data[4];

extern int kernel_end; 
static uint32_t *FAT = (uint32_t *)&kernel_end;
static uint32_t *FAT_end;

// static int fat_modified = 0;

//////////////////////////////////////////////////////////////////////////
// 
// Traverse and modify a local FAT.
//
//////////////////////////////////////////////////////////////////////////

// Lookup the next cluster that is part of this file. This may be a local
// or distributed address, TODO add a multiplexing argument since this
// information cannot be inferred from the local FAT.
//
uint32_t fat32_next_cluster(uint32_t cur_cluster_no) {
	if(!did_init) {
		panic("Attempted to read FAT before fat32 initialization.\n");
	}

	// Notice that for the purposes of traversal, we do not use the top 4
	// bits of the cluster number at all.
	uint32_t fat_entry = FAT[cur_cluster_no];
	return CLUSTER_NO(fat_entry);
}

// Find the first free cluster in the local FAT table
//
uint32_t find_free_local_cluster() {
	if(!did_init) {
		panic("Attempted to inspect the filesystem before initialization.\n");
	}
	for(uint32_t *p = FAT + 2; p != FAT_end; ++p) {
		if(*p == 0) {
			return ((uintptr_t)p - (uintptr_t)FAT)/sizeof(uint32_t);
		}
	}
	return -1;
}

// Allocate the first free cluster by markign it as end of file chain.
//
uint32_t alloc_free_local_cluster() {
	uint32_t out = find_free_local_cluster();
	FAT[CLUSTER_NO(out)] = CLUSTER_NO(-1);

	// Write the dirty fat sector back to disk.
	uint32_t index_in_fat = CLUSTER_NO(out);

	static uint32_t words_per_sector = SD_SECTOR_SIZE / sizeof(uint32_t);

	uint32_t sector_in_fat = index_in_fat / words_per_sector;
	uint32_t *buf_to_write = FAT + (sector_in_fat * words_per_sector);

	writesector((unsigned char *)buf_to_write,
			sector_in_fat + vol_data[chosen_partition].fat_begin_lba, 1);

	// Write all zeros to the first sector of the allocd file.
	static struct __attribute__((packed)) {
		uint8_t buf[512];
	} blank_sector = {0};

	writesector((unsigned char *)&blank_sector, 
			cluster_no_to_lba(CLUSTER_NO(out)), 1);

	return out;
}

#if 0
// We pass in the cluster number of the final cluster in a file chain.
// This function will allocate a new cluster, add it to the chain,
// and return the alloc'd cluster number.
//
uint32_t fat32_extend_chain_by_1(uint32_t cur_cluster_no) {
	if(!did_init) {
		panic("Attempted to write to the FAT before fat32 initialization.\n");
	}

	if(fat32_next_cluster(cur_cluster_no) != 
			FAT32_TERMINAL_CLUSTER_NO) {

		panic("This is not the end of the cluster chain.\n");
	}

	// Allocate a new cluster, add it to the chain.
	uint32_t next_no = find_free_local_cluster();
	FAT[cur_cluster_no] = next_no;
	FAT[next_no] = -1;

	return next_no;
}
#endif

void follow_file_chain(uint32_t start_cluster_no) {
	while(start_cluster_no != -1) {
		printk("%d ", start_cluster_no);
		start_cluster_no = FAT[CLUSTER_NO(start_cluster_no)];
	}
	printk("%d\n", start_cluster_no);
}

// Point an entry in the fat to the next cluster, and write the dirty
// sector back to disk.
void fat32_point_fat(uint32_t prev, uint32_t next) {
	FAT[prev] = next;

	uint32_t index_in_fat = CLUSTER_NO(prev);

	static uint32_t words_per_sector = SD_SECTOR_SIZE / sizeof(uint32_t);

	uint32_t sector_in_fat = index_in_fat / words_per_sector;
	uint32_t *buf_to_write = FAT + (words_per_sector * sector_in_fat);

	writesector((unsigned char *)buf_to_write, 
			sector_in_fat + vol_data[chosen_partition].fat_begin_lba, 1);
}

uint32_t alloc_local_file_at(
		struct dir_record *dr, int num_clusters, char const *name
	) {

	for(int j = 0; j < 11; ++j) { 
		dr->dir_name[j] = name[j];
	}

	uint32_t prev_cluster, allocd_cluster, out;
	out = prev_cluster = allocd_cluster = alloc_free_local_cluster();
	--num_clusters;

	dr->dir_first_cluster_low = ((uint16_t *)&allocd_cluster)[0];
	dr->dir_first_cluster_high = ((uint16_t *)&allocd_cluster)[1];

	// No special attribute values (no sys, ro, isdir, etc.)
	dr->dir_attr = 0;

	// Alloc the rest of the file and populate the FAT.
#if DEBUG
	printk("Creating file @ clusters: %d ", allocd_cluster);
#endif
	while(num_clusters > 0) {
		allocd_cluster = alloc_free_local_cluster();
		fat32_point_fat(CLUSTER_NO(prev_cluster), CLUSTER_NO(allocd_cluster));

#if DEBUG
		printk("%d ", allocd_cluster);
#endif
		
		prev_cluster = allocd_cluster;
		--num_clusters;
	}
#if DEBUG
	printk("\n");
#endif

	return out;
}

void fat32_dump_dir_rec(struct dir_record *);

uint32_t fat32_alloc_local_file(uint32_t num_clusters, char const *name) {
	if(!did_init) {
		panic("Attempted to write to fat32 before init.\n");
	} 

#if DEBUG
	printk("======================================\n");
	printk("== PERFORMING A LOCAL ALLOC ON ROOT ==\n");
	printk("======================================\n");
#endif

#if DEBUG
	printk("Sectors per cluster: %d\n", 
			all_vol_ids[chosen_partition].sectors_per_cluster);
#endif

	// We will find a free directory entry in the root directory. Then, we
	// will allocate and make a cluster chain of size num_clusters in the
	// fat, and point the directory entry to the head of the chain.
	uint32_t cur_dir_cluster_no, prev_dir_cluster_no; 
	
	cur_dir_cluster_no = prev_dir_cluster_no = 
		vol_data[chosen_partition].root_dir_first_cluster;

	uint32_t records_per_sector = SD_SECTOR_SIZE / sizeof(struct dir_record);

	struct dir_record records[records_per_sector];

	while(cur_dir_cluster_no != -1) {

#if DEBUG
		printk("Reading directory cluster number: %d.\n",
				cur_dir_cluster_no);

		printk("Cluster start lba: %d.\n", 
				cluster_no_to_lba(cur_dir_cluster_no));

		printk("First sector not part of this cluster: %d.\n",
				cluster_no_to_lba(cur_dir_cluster_no) + 
				all_vol_ids[chosen_partition].sectors_per_cluster);
#endif

		for(int sector_no = 0;
				sector_no < all_vol_ids[chosen_partition].sectors_per_cluster;
				++sector_no) {

#if DEBUG
			printk("within cluster, reading sector %d.\n", 
					cluster_no_to_lba(cur_dir_cluster_no) + sector_no);
#endif

			readsector(cluster_no_to_lba(cur_dir_cluster_no) + sector_no,
					(unsigned char *)records, 1);

			for(int i = 0; i < records_per_sector; ++i) {

#if DEBUG
				if(!IS_LFN(records[i].dir_attr)) {
					printk("dir entry %d:\n", i);
					printk("idx: %d\n", i);
					fat32_dump_dir_rec(&records[i]);
				}
#endif
				
				if(!IS_LFN(records[i].dir_attr) && IS_TERMINAL(records[i])) {
					
#if DEBUG
					printk("Handling the terminal now.\n");

					printk("Here are the contents of the sector to be written back, before the terminal is modified.\n");
					for(int j = 0; j < records_per_sector; ++j) {
						fat32_dump_dir_rec(&records[j]);
						if(IS_TERMINAL(records[j])) { break; }
					}
#endif

					uint32_t file_first_cluster_no = 
						alloc_local_file_at(&records[i], num_clusters, name);

#if DEBUG
					printk("Now updating the terminal entry.\n");

					printk("records_per_sector: %d, i: %d\n", records_per_sector, i);
#endif

					if(i == records_per_sector - 1) {
						
#if DEBUG
						printk("The inserted entry lives at a sector boundary, we now update the next sector of the cluster.\n");
#endif

						// We are at a sector boundary
						if(sector_no == 
								all_vol_ids[chosen_partition].sectors_per_cluster - 1) {

							// We are at a cluster boundary. No need to allocate a new
							// cluster to store the terminal. If we exit the while,
							// and a terminal has not been found, we will allocate a 
							// new cluster only then.

#if DEBUG
							printk("The entry lives at a cluster boundary, no need to write the next sector.\n");
							
							printk("writing to sector: %d", 
									cluster_no_to_lba(cur_dir_cluster_no) + sector_no); 
#endif
							writesector((unsigned char *)records,
								cluster_no_to_lba(cur_dir_cluster_no) + sector_no, 1);

							return file_first_cluster_no;

						} else {

							// The next sector is guaranteed to have useless information,
							// just write back char[sd_sector_size] = {0, rand*511} 
							// back to the next sector, after writing back the current
							// sector.

#if DEBUG
							printk("We are not at a cluster boundary, we need to write the terminal to the next sector.\n");
							printk("Writing displayed contents to %d.\n",
									cluster_no_to_lba(cur_dir_cluster_no) + sector_no);
							printk("Writing terminal sector to %d.\n",
									cluster_no_to_lba(cur_dir_cluster_no) + sector_no + 1);
#endif

							writesector((unsigned char *)records,
								cluster_no_to_lba(cur_dir_cluster_no) + sector_no, 1);

							((uint8_t *)&records[0])[0] = 0;
							writesector((unsigned char *)records,
								cluster_no_to_lba(cur_dir_cluster_no) + sector_no + 1, 1);

							return file_first_cluster_no;
						}
					} else {

						// Modify next entry to contain the terminal, and write the
						// dirty sector back to disk.

#if DEBUG
						printk("The inserted dir entry is not at a sector boundary.\n");
						printk("Here is the sector to write back, after modifying the terminal:\n");
#endif

						((uint8_t *)&records[i + 1])[0] = 0;
						writesector((unsigned char *)records, 
							cluster_no_to_lba(cur_dir_cluster_no) + sector_no, 1);
						
#if DEBUG
						for(int j = 0; j < records_per_sector; ++j) {
							fat32_dump_dir_rec(&records[j]);
							if(IS_TERMINAL(records[j])) { break; }
						}

						printk("Writing to sector %d.\n",
								cluster_no_to_lba(cur_dir_cluster_no) + sector_no);
#endif
						return file_first_cluster_no;
					}
				}
				
			}
		}

		prev_dir_cluster_no = cur_dir_cluster_no;
		cur_dir_cluster_no = fat32_next_cluster(cur_dir_cluster_no);
	}

	if(cur_dir_cluster_no == CLUSTER_NO(-1)) {

#if DEBUG
		printk("We have reached the end of the directory cluster chain, allocing a new cluster.\n");
#endif

		// We have reached the end of the root directory without finding
		// a free entry. So, allocate a new cluster for the root directory,
		// and add it to the root directory's cluster chain. alloc_cluster
		// marks the allocd cluster as terminal in the fat.

		uint32_t allocd_cluster = alloc_free_local_cluster();
		fat32_point_fat(CLUSTER_NO(prev_dir_cluster_no), CLUSTER_NO(allocd_cluster));

#if DEBUG
		printk("Newly allocd sector no: %d.\n", allocd_cluster);
#endif
		
		// Now, set the first sector of the newly allocated cluster to
		// an empty sector with just the allocated file entry, as well as 
		// a terminal, then write this sector to the first sector of the
		// allocated cluster.

		uint32_t file_first_cluster_no = 
			alloc_local_file_at(&records[0], num_clusters, name);

		((uint8_t *)(&records[1]))[0] = 0;

#if DEBUG
		printk("Allocd file:\n");
		fat32_dump_dir_rec(&records[0]);
#endif

#if DEBUG
		printk("Writing to sector %d.\n",
				cluster_no_to_lba(CLUSTER_NO(allocd_cluster)));
#endif

		writesector((unsigned char *)records,
			cluster_no_to_lba(CLUSTER_NO(allocd_cluster)), 1);

		return file_first_cluster_no;
	}

	return -1;
}

#if 0
// Allocates a file of size num_clusters on the local disk, and returns
// the starting cluster number. The file is always thrown into the
// root directory of the local system.
//
uint32_t fat32_alloc_local_file(uint32_t num_clusters, char const *name) {
	if (!did_init) {
		panic("Attempted to write to filesystem before initialization.\n");
	}

	// Find a free directory entry in the root directory, and assign a 
	// free cluster to it.
	uint32_t cur_dir_cluster_no = vol_data[chosen_partition].root_dir_first_cluster;
	uint32_t prev_dir_cluster_no = cur_dir_cluster_no;

	uint32_t records_per_cluster =
			(vol_data[chosen_partition].sectors_per_cluster * SD_SECTOR_SIZE) 
			/ sizeof(struct dir_record);

	struct dir_record *cluster_data = (struct dir_record *)FAT_end;
	while(cur_dir_cluster_no != -1) {

		printk("Reading directory cluster no %d.\n", cur_dir_cluster_no);
		read_cluster(cur_dir_cluster_no, (uint8_t *)cluster_data);

		printk("This cluster no contains sectors: ");
		for(unsigned i = 0; i < all_vol_ids[chosen_partition].sectors_per_cluster; ++i) {
			printk("%d ", cluster_no_to_lba(cur_dir_cluster_no) + i);
		}
		printk("\n");
		
		printk("Iterating through dir contents of this sector.\n");
		for(unsigned long i = 0; i < records_per_cluster; ++i) {
			if(IS_TERMINAL(cluster_data[i])) {
				printk("dir record at index %d is a terminal.\n", i);

				// Found free space, we will insert the file here. Allocate a new
				// cluster for the file data, and add the cluster number to this
				// entry.
				uint32_t file_first_cluster_no = alloc_local_file_at(cluster_data + i, num_clusters, name);

				printk("Just finished modifying dir record to reflect newly allocated file.\n");
				printk("Here is the new dir record:\n");

				// TO DELETE /////////////////////////////////
				// TO DELETE /////////////////////////////////
				// TO DELETE /////////////////////////////////
				for(int j = 0; j < sizeof(cluster_data[i].dir_name); ++j) {
					printk("%c", cluster_data[i].dir_name[j]);
				}
				printk("\t");
				uint32_t cluster_no;
				((uint16_t *)&cluster_no)[0] = cluster_data[i].dir_first_cluster_low;
				((uint16_t *)&cluster_no)[1] = cluster_data[i].dir_first_cluster_high;
				if(cluster_no == 0) {
					printk("attempting to inspect cluster no < 2, aborting.\n");
				} else {
					putk("clusters: ");
					do {
						printk("%d [%x] ", cluster_no, cluster_no);
						cluster_no = fat32_next_cluster(cluster_no);

						if(cluster_no <= 2 && cluster_no != FAT32_TERMINAL_CLUSTER_NO) {
							putk("PRE-ROOT ");
						}

					} while(cluster_no != FAT32_TERMINAL_CLUSTER_NO && 
									cluster_no > 2);
					putk("\n");
				}
				// TO DELETE /////////////////////////////////
				// TO DELETE /////////////////////////////////
				// TO DELETE /////////////////////////////////
			
				// Change the terminal.
				if(i + 1 != records_per_cluster) {
					*((uint8_t *)&cluster_data[i + 1]) = 0;
				}
				printk("Updated i + 1 now contains the terminal is true: %d\n",
						((uint8_t *)&cluster_data[i + 1])[0] == 0);

				// Write back dirty sector.
				uint32_t dirty_lba = cluster_no_to_lba(cur_dir_cluster_no)
					+ (i * sizeof(struct dir_record) / SD_SECTOR_SIZE);

				uint32_t records_per_sector = SD_SECTOR_SIZE / sizeof(struct dir_record);

				struct dir_record *start_dirty = cluster_data + 
					records_per_sector * (i / records_per_sector);
				(void)writesector((const unsigned char *)start_dirty, dirty_lba, 1);
				printk("Writing dirty lba: %u\n", dirty_lba);
				printk("Dir cluster no: %u, cluster start lba: %d\n", 
						cur_dir_cluster_no, 
						cluster_no_to_lba(cur_dir_cluster_no));

				printk("Contents of the sector bring written back:\n");
				
				struct dir_record *start = cluster_data + 
					records_per_sector * (i / records_per_sector);

				for(struct dir_record *s = start; 
						s != start + records_per_sector && !IS_TERMINAL(*s);
						++s) {

					// TO DELETE /////////////////////////////////
					// TO DELETE /////////////////////////////////
					// TO DELETE /////////////////////////////////
					for(int j = 0; j < sizeof(s->dir_name); ++j) {
						printk("%c", s->dir_name[j]);
					}
					printk("\t");
					uint32_t cluster_no;
					((uint16_t *)&cluster_no)[0] = s->dir_first_cluster_low;
					((uint16_t *)&cluster_no)[1] = s->dir_first_cluster_high;
					if(cluster_no == 0) {
						printk("attempting to inspect cluster no < 2, aborting.\n");
					} else {
						putk("clusters: ");
						do {
							printk("%d [%x] ", cluster_no, cluster_no);
							cluster_no = fat32_next_cluster(cluster_no);

							if(cluster_no <= 2 && cluster_no != FAT32_TERMINAL_CLUSTER_NO) {
								putk("PRE-ROOT ");
							}

						} while(cluster_no != FAT32_TERMINAL_CLUSTER_NO && 
										cluster_no > 2);
						putk("\n");
					}
					// TO DELETE /////////////////////////////////
					// TO DELETE /////////////////////////////////
					// TO DELETE /////////////////////////////////
					
				}

				follow_file_chain(file_first_cluster_no);

				return file_first_cluster_no;
			} else {
				printk("dir record at index %d is not a terminal.\n", i);
			}
		}
		
		prev_dir_cluster_no = cur_dir_cluster_no;
		cur_dir_cluster_no = fat32_next_cluster(cur_dir_cluster_no);
	}

	if(cur_dir_cluster_no == CLUSTER_NO(-1)) {

		// We have reached the end of the root directory without finding
		// a free entry. So, allocate a new cluster for the root directory,
		// and add it to the root directory's cluster chain.

		uint32_t allocd_cluster = alloc_free_local_cluster();
		FAT[CLUSTER_NO(prev_dir_cluster_no)] = CLUSTER_NO(allocd_cluster);

		// Now, set the end of the dir to the entry, and write it to the
		// first sector of the allocated cluster.
		uint32_t file_first_cluster_no = alloc_local_file_at(cluster_data, num_clusters, name);

		// Set terminal at appropriate location.
		*((uint8_t *)&cluster_data[1]) = 0;

		// Write back dirty sector. The dirty sector is the first sector
		// of this cluster.
		uint32_t dirty_lba = cluster_no_to_lba(CLUSTER_NO(allocd_cluster));
		(void)writesector((const unsigned char *)cluster_data, dirty_lba, 1);
		printk("Writing dirty sector %u\n", dirty_lba);
		printk("Dir cluster no: %u, cluster start lba: %d\n", 
				cur_dir_cluster_no, 
				cluster_no_to_lba(cur_dir_cluster_no));

		follow_file_chain(file_first_cluster_no);

		return file_first_cluster_no;
	}

	return -1;
}
#endif

#if 0
// Returns the starting cluster of the newly allocated file. To find
// the dir record in the root, just iterate through all dir records
// until you find one with the correct cluster number.
uint32_t alloc_file_in_root() {
	

	// This should be extremely rare.
	if(cur_cluster_no == -1) {
		uint32_t allocd_cluster_no = 
			fat32_extend_chain_by_1(prev_cluster_no);

		// Allocate a new cluster for a file, and add it to the first entry
		// in this allocated directory cluster.
	}
}
#endif

void dump_root_content() {
	if(!did_init) {
		panic("Attempted to dump the filesystem before fat32 init.\n");
	}

	uint32_t cluster_no = FAT32_ROOT_CLUSTER_NO;

	uint32_t records_per_sector = SD_SECTOR_SIZE / sizeof(struct dir_record);
	struct dir_record records[records_per_sector];

	while(cluster_no != -1) {

		for(int sector_no = 0; 
				sector_no < all_vol_ids[chosen_partition].sectors_per_cluster;
				++sector_no) {

			readsector(cluster_no_to_lba(cluster_no) + sector_no,
					(unsigned char *)records, 1);

			for(int i = 0; i < records_per_sector; ++i) {
				if(!IS_LFN(records[i].dir_attr)) {
					fat32_dump_dir_rec(&records[i]);
				}

				if(IS_TERMINAL(records[i])) { return; }
			}
		}

		cluster_no = fat32_next_cluster(cluster_no);
	}
}

uint32_t fat32_get_cluster_no(char const *filename) {
	if(!did_init) {
		panic("Attempted to search the filesystem before fat32 init.\n");
	}

	uint32_t cluster_no = FAT32_ROOT_CLUSTER_NO;

	uint32_t records_per_sector = SD_SECTOR_SIZE / sizeof(struct dir_record);
	struct dir_record records[records_per_sector];

	while(cluster_no != -1) {

		for(int sector_no = 0; 
				sector_no < all_vol_ids[chosen_partition].sectors_per_cluster;
				++sector_no) {

			readsector(cluster_no_to_lba(cluster_no) + sector_no,
					(unsigned char *)records, 1);

			for(int i = 0; i < records_per_sector; ++i) {
				if(!IS_LFN(records[i].dir_attr)) {
					if(IS_TERMINAL(records[i])) { return -1; }

					if(strncmp(records[i].dir_name, filename, 11) == 0) {

						uint32_t file_start_cluster_no;

						((uint16_t *)&file_start_cluster_no)[0] = 
							records[i].dir_first_cluster_low;

						((uint16_t *)&file_start_cluster_no)[1] = 
							records[i].dir_first_cluster_high;

						return file_start_cluster_no;
					}
				}
			}
		}
		cluster_no = fat32_next_cluster(cluster_no);
	}

	return -1;
}

#if 0
uint32_t fat32_find_cluster_no(char const *name, uint32_t dir_cluster_no) {
	if(!did_init) {
		panic("Attempted to perform an FS query before initialization.\n");
	}

	uint32_t records_per_sector = SD_SECTOR_SIZE / sizeof(struct dir_record);
	struct dir_record records[records_per_sector];
	
	while(dir_cluster_no != -1) {

		for(int sector_no = 0; 
				sector_no < all_vol_ids[chosen_partition].sectors_per_cluster;
				++sector_no) {

			readsector(cluster_no_to_lba(dir_cluster_no) + sector_no, 
								 (unsigned char *)records, 1);

			for(int rec_idx = 0; rec_idx < records_per_sector; ++rec_idx) {
				if(IS_TERMINAL(records[rec_idx])) {
					return -1;
				} else if(!IS_LFN(records[rec_idx].dir_attr)) {


					if(strncmp(records[rec_idx].dir_name, name, 11) == 0) {
						uint32_t cluster_no;

						((uint16_t *)&cluster_no)[0] = 
							records[rec_idx].dir_first_cluster_low;

						((uint16_t *)&cluster_no)[1] = 
							records[rec_idx].dir_first_cluster_high;

						return cluster_no;
					}
				
				}
			}			
		}

		dir_cluster_no = fat32_next_cluster(dir_cluster_no);
	}

	return -1;
}
#endif

//////////////////////////////////////////////////////////////////////////
//
// Reading and modifying the actual disk.
//
//////////////////////////////////////////////////////////////////////////

// Determine the first sector number that is part of this cluster.
//
uint32_t cluster_no_to_lba(uint32_t cluster_no) {
	return
		vol_data[chosen_partition].cluster_begin_lba + 
		(cluster_no - vol_data[chosen_partition].root_dir_first_cluster) *
		vol_data[chosen_partition].sectors_per_cluster;
}

void read_cluster(uint32_t cluster_no, uint8_t *dst) {
	uint32_t lba = cluster_no_to_lba(cluster_no);
	printk("fat32: reading cluster %d (lba %d)\n", cluster_no, lba);
	for(int i = 0;
			i < all_vol_ids[chosen_partition].sectors_per_cluster;
			++i) {

		readsector(lba++, dst, 1);
		dst += SD_SECTOR_SIZE;
	}
}

void write_cluster(uint32_t cluster_no, uint8_t *src) {
	if(!did_init) {
		panic("Attempted to inspect the filesystem before initialization.\n");
	}
	uint32_t lba = cluster_no_to_lba(cluster_no);
	printk("fat32: writing cluster %d (lba %d)\n", cluster_no, lba);
	for(int i = 0; 
			i < all_vol_ids[chosen_partition].sectors_per_cluster; 
			++i) {

		writesector(src, lba++, 1);
		src += SD_SECTOR_SIZE;
	}
}

//////////////////////////////////////////////////////////////////////////
// 
// Inspect FAT metadata routines.
//
//////////////////////////////////////////////////////////////////////////

void fat32_dump_vol_id(struct volume_id *vol_id) {
	printk("+-----------------------\n");
	printk("|bytes per sector: %d\n", vol_id->bytes_per_sector);
	printk("|sectors per cluster: %d\n", vol_id->sectors_per_cluster);
	printk("|num reseved sectors: %d\n", vol_id->num_reserved_sectors);
	printk("|num fats: %d\n", vol_id->num_fats);
	printk("|sectors per fat: %d\n", vol_id->sectors_per_fat);
	printk("|root dir first cluster: %d\n", 
			vol_id->root_dir_first_cluster);
	printk("|magic: %x %x\n", vol_id->magic1, vol_id->magic2);
	printk("+-----------------------\n");
}

void fat32_dump_dir_rec(struct dir_record *d) {
	if(((uint8_t *)d)[0] == 0) {
		printk("+--------+\n");
		printk("|TERMINAL|\n");
		printk("+--------+\n");
	} else {
		printk("+-----\n");
		printk("|name: ");
		for(int i = 0; i < sizeof(d->dir_name); ++i) {
			printk("%c", d->dir_name[i]);
		}
		printk("\n");
		printk("|attr: %x\n", d->dir_attr);
		uint32_t cluster_no;
		((uint16_t *)&cluster_no)[0] = d->dir_first_cluster_low;
		((uint16_t *)&cluster_no)[1] = d->dir_first_cluster_high;

		printk("|file size: %d\n", d->dir_file_size);
		printk("|ordered cluster list: ");
		while(1) {
			if(cluster_no == FAT32_TERMINAL_CLUSTER_NO) {
				printk("TERM\n");
				break;
			} else if(cluster_no <= 2) {
				putk("PRE-ROOT\n");
				break;
			} else {
				printk("%d [%x] ", cluster_no, cluster_no);
			}
			cluster_no = fat32_next_cluster(cluster_no);
		}
		printk("+--\n");
	}
}

#if 0
int fat32_dump_dir_recs(dir_sector *ds) {
	struct dir_record *d;
	for(int i = 0; i < sizeof(dir_sector)/sizeof(struct dir_record); ++i) {
		d = (struct dir_record *)ds + i;
		if(((uint8_t *)d)[0] == 0) return 1;
		printk("+-----\n");
		printk("|name: ");
		for(int i = 0; i < sizeof(d->dir_name); ++i) {
			printk("%c", d->dir_name[i]);
		}
		printk("\n");
		printk("|attr: %x\n", d->dir_attr);
		uint32_t first_cluster_number;
		((uint16_t *)&first_cluster_number)[0] = d->dir_first_cluster_low;
		((uint16_t *)&first_cluster_number)[1] = d->dir_first_cluster_high;

		printk("|first cluster number: %u\n", first_cluster_number);
		printk("|file size: %d\n", d->dir_file_size);
		printk("+--\n");
	}
	return 0;
}
#endif

void print_dir_ent_attr(struct dir_record *d) {
	printk("ro<%d> hidden<%d> sys<%d> volid<%d> dir<%d>\n",
		IS_RO(d->dir_attr), 
		IS_HIDDEN(d->dir_attr), 
		IS_SYSTEM(d->dir_attr),
		IS_VOLID(d->dir_attr),
		IS_DIR(d->dir_attr),
		IS_ARCHIVE(d->dir_attr)
	);
}

// Given a cluster number representing a directory, dump the directory
// contents of that cluster. This should ignore LFN entries, and print
// a char<12> along with an associated start cluster for each file.
//
void fat32_inspect_dir(uint32_t cluster_no) {
	if(!did_init) {
		panic("Attempted to inspect the filesystem before initialization.\n");
	}

	uint8_t *dst = (uint8_t *)FAT_end;
	uint8_t *saved_begin = dst;
	uint32_t bytes_per_cluster = 
		vol_data[chosen_partition].sectors_per_cluster * 
		all_vol_ids[chosen_partition].bytes_per_sector;

	int num_clusters = 0;
	do {
		read_cluster(cluster_no, dst);
		num_clusters++;
		cluster_no = fat32_next_cluster(cluster_no);
		dst += bytes_per_cluster;
	} while(cluster_no != FAT32_TERMINAL_CLUSTER_NO);

	printk("Files in directory %d [%x]:\n", cluster_no, cluster_no);
	struct dir_record *dir_rec;
	for(dir_rec = (struct dir_record *)saved_begin;
			dir_rec != (struct dir_record *)((uint8_t *)FAT_end + bytes_per_cluster * num_clusters);
			dir_rec += 1) {

		if(IS_TERMINAL(*dir_rec)) { break; }

		if(!IS_LFN(dir_rec->dir_attr)) {
			printk("[idx: %d] ", (dir_rec - (struct dir_record *)saved_begin));

			printk("<");
			for(int i = 0; i < sizeof(dir_rec->dir_name); ++i) {
				printk("%c", dir_rec->dir_name[i]);
			}
			printk(">");
			printk("\t");
			uint32_t cluster_no;
			((uint16_t *)&cluster_no)[0] = dir_rec->dir_first_cluster_low;
			((uint16_t *)&cluster_no)[1] = dir_rec->dir_first_cluster_high;
			if(cluster_no == 0) {
				printk("attempting to inspect cluster no < 2, aborting.\n");
			} else {
				putk("clusters: ");
				do {
					printk("%d [%x] ", cluster_no, cluster_no);
					cluster_no = fat32_next_cluster(cluster_no);

					if(cluster_no <= 2 && cluster_no != FAT32_TERMINAL_CLUSTER_NO) {
						putk("PRE-ROOT ");
					}

				} while(cluster_no != FAT32_TERMINAL_CLUSTER_NO && 
								cluster_no > 2);
				putk("\n");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
//
// ...
//
//////////////////////////////////////////////////////////////////////////


int fat32_init(
		int (*read)(unsigned, unsigned char *, unsigned), 
		int (*write)(const unsigned char *, unsigned int, unsigned int)) {

	readsector = read;
	writesector = write;

	(void)readsector(MBR_SECTOR, (unsigned char *)&boot_sector, 1);

	int num_valid_partitions = 0;
	for(int i = 0; i < 4; ++i) {
		struct partition *s = &boot_sector.partitions[i];
		if(s->num_sectors != 0) {
			chosen_partition = i;
			if(++num_valid_partitions == 2) {
				panic("Found more than 1 valid partitions on this pi, don't know which to choose.\n");
			} 		
			partitions_in_use |= 1<<i;

			(void)readsector(s->lba_begin, (unsigned char *)&all_vol_ids[i], 1);

			if((uint32_t)all_vol_ids[i].bytes_per_sector != SD_SECTOR_SIZE) {
				printk("bps: %d, sector size: %d\n", 
						all_vol_ids[i].bytes_per_sector, SD_SECTOR_SIZE);
				panic("Partition uses different sector size than sd driver.\n");
			}

			// printk("+-----------------------\n");
			// printk("|Fat32 parition [ %d ]\n", i);
			// printk("|start lba: %d\n", boot_sector.partitions[i].lba_begin);
			// printk("|num sectors: %d\n", boot_sector.partitions[i].num_sectors);
			// fat32_dump_vol_id(&all_vol_ids[i]);

			// Init fat per-volume relevant data.
			vol_data[i].fat_begin_lba = 
				s->lba_begin + all_vol_ids[i].num_reserved_sectors;

			vol_data[i].cluster_begin_lba = 
				vol_data[i].fat_begin_lba + 
				(all_vol_ids[i].sectors_per_fat * all_vol_ids[i].num_fats);

			vol_data[i].sectors_per_cluster =
				all_vol_ids[i].sectors_per_cluster;

			vol_data[i].root_dir_first_cluster = 
				all_vol_ids[i].root_dir_first_cluster;
		}
	}

	// Read the FAT into main memory.
	struct volume_id *v = &all_vol_ids[chosen_partition];
	struct fat_volume_data *vd = &vol_data[chosen_partition];

	struct sector {
		uint32_t data[SD_SECTOR_SIZE];
	};

	// printk("Reading the FAT into main memory...\n");
	int i = 0;
	for(; i < v->sectors_per_fat; ++i) {
		struct sector *fat_s = (struct sector *)FAT + i;
		readsector(
				vd->fat_begin_lba + i,
				(unsigned char *)fat_s,
				1);
	}
	FAT_end = (uint32_t *)((uint8_t *)FAT + 
			(v->bytes_per_sector * v->sectors_per_fat));
	// printk("Done reading %d FAT sectors into memory.\n", i);

	did_init = 1;
	return 0;
}

#if 0
void fat32_shutdown() {
	struct sector {
		uint32_t data[SD_SECTOR_SIZE];
	};

	if(fat_modified) {
		struct volume_id *v = &all_vol_ids[chosen_partition];
		struct fat_volume_data *vd = &vol_data[chosen_partition];

		printk("Writing the fat back to memory... DO NOT SHUTDOWN PI\n");
		for(int i = 0; i < v->sectors_per_fat; ++i) {
			struct sector *fat_s = (struct sector *)FAT + i;
			writesector(
					(unsigned char *)fat_s,
					vd->fat_begin_lba + i,
					1);
		}
		printk("Done writing FAT to disk, it is safe to reset the pi.\n");
	}
}
#endif

#if 0

	
	uint32_t allocd_cluster_no = find_free_cluster();
	
	FAT[prev_cluster_no] = allocd_cluster_no;
	FAT[allocd_cluster_no] = -1;
	
	
	struct dir_record *root_dir_data = (uint8_t *)FAT_end;
	read_cluster(
			vol_data[chosen_partition].root_dir_first_cluster, root_dir_data);


	

		

    // Read the FAT into main memory
    struct volume_id *v = &all_vol_ids[chosen_partition];
    struct fat_volume_data *vd = &vol_data[chosen_partition];

    // Calcultes bytes per cluster by getting the number of sectors per cluster then the number
    // of bytes per sector
    uint32_t bytes_per_cluster =
        vol_data[chosen_partition].sectors_per_cluster *
        all_vol_ids[chosen_partition].bytes_per_sector;

    // Get the first cluster number of the root directory for the chosen partition
    uint32_t cluster_no = vol_data[chosen_partition].root_dir_first_cluster;

    // Points to first spot of memory after FAT (need to check if I have to skip anything for special
    // reserved... I don't think so but just a TO DO check)
    uint8_t *dir_cluster = (uint8_t *)FAT_end;

    // Reads the first cluster and calls it a dir cluster so we have a place to start looking
    // (this all feels a bit clunky to me... can probably be optimized)
    read_cluster(cluster_no, dir_cluster);

    struct dir_record *dr = (struct dir_record *)dir_cluster;
    struct dir_record *free_entry = NULL;

    // Find a free directory entry
    for (int i = 0; i < sizeof(dir_sector) / sizeof(struct dir_record); ++i) {
        if (((uint8_t *)dr)[0] == 0) {
            free_entry = dr;
            break;
        }
        dr++;
    }

    if (!free_entry) {
        panic("No free directory entry found.\n");
    }

    // Set all bytes in the memory block pointed to by free_entry to zero
    memset(free_entry, 0, sizeof(struct dir_record));
    // Copy the file name into the dir_name field of the directory entry
    strcpy(free_entry->dir_name, filename, sizeof(free_entry->dir_name));

    // Set attr to be Archive since it's been changed since the last backup... but since
    // we're not doing any error checking this might be wrong and should just be "system"
    // function...
    free_entry->dir_attr = ATTR_ARCHIVE;
    free_entry->dir_file_size = size;
    free_entry->dir_first_cluster_low = 0;
    free_entry->dir_first_cluster_high = 0;

    uint32_t first_cluster = 0;
    uint32_t prev_cluster = 0;

    // Write data to clusters
    while (size > 0) {
        uint32_t free_cluster = find_free_cluster();

        // Handle error if no free clusters
        if (!free_cluster) {
            panic("No free clusters available.\n");
        }
        
        if (first_cluster == 0) {
            first_cluster = free_cluster;
        }

        if (prev_cluster != 0) {
            FAT[prev_cluster] = free_cluster;
        }

        uint32_t bytes_to_write = size < bytes_per_cluster ? size : bytes_per_cluster;
        write_cluster(free_cluster, data);

        size -= bytes_to_write;
        data += bytes_to_write;
        prev_cluster = free_cluster;
    }

    // Mark the last cluster as the end of file
    FAT[prev_cluster] = FAT32_TERMINAL_CLUSTER_NO;

    // Update the directory entry with the starting cluster
    free_entry->dir_first_cluster_low = first_cluster & 0xFFFF;
    free_entry->dir_first_cluster_high = (first_cluster >> 16) & 0xFFFF;

    // Write the updated directory cluster back to the disk
    write_cluster(cluster_no, dir_cluster);

    // Write the updated FAT back to the disk
    for (int i = 0; i < v->sectors_per_fat; ++i) {
        uint32_t *fat_s = FAT + (i * (SD_SECTOR_SIZE / sizeof(uint32_t)));
        writesector((const unsigned char *)fat_s, vd->fat_begin_lba + i, 1);
    }
}

#endif

