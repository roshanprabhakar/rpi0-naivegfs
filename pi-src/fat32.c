#include "rpi.h"
#include "fat32.h"
//#include <string.h>

// Reads (sector lba, buffer dst, number of sectors)
static int (*readsector)(unsigned lba, unsigned char *buf, unsigned num);
static int (*writesector)(const unsigned char *buf, unsigned int lba, unsigned int num);
static int did_init = 0;
static uint8_t partitions_in_use = 0;
static int chosen_partition;

static struct mbr boot_sector;
static struct volume_id all_vol_ids[4];
static struct fat_volume_data vol_data[4];

extern int kernel_end; // The address of this variable is defined by the
											 // linker script. Take its address to determine 
											 // where in memory the kernel ends. As a hack to
											 // get around the need to dynamically allocate 
											 // memory for the FAT, we will just place the
											 // FAT here, and initialize it once at boot.
											 // This works since we never need to dynamic
											 // allocation again. If we do, we either get a 
											 // working malloc together quickly, or we are 
											 // fucked.
static uint32_t *FAT = (uint32_t *)&kernel_end;
static uint32_t *FAT_end;

static int fat_init = 0;

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

uint32_t fat32_next_cluster(uint32_t cur_cluster_no) {
	if(!did_init) {
		panic("Attempted to read FAT before fat32 initialization.\n");
	}

	uint32_t fat_entry = FAT[cur_cluster_no];
	uint32_t cluster_no = fat_entry & (((uint32_t)-1)>>4);
	return cluster_no;	
}

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

// TODO in the is_lfn section, convert uint16_t utf16 chars into
// uint8_t ascii chars. We need to do this without a library. 
// This should work after that though.
struct dir_record *print_dir_record(
		struct dir_record *dr
	) {
	
	// This dirrec indicates that no more will follow, and holds
	// no local file info.
	if(IS_TERMINAL(*dr)) { return NULL; }

	// Check whether this entry is part of a long file name (LFN)
	int is_lfn = IS_LFN(dr->dir_attr);
	printk("LFN<%d>", is_lfn);

    if(is_lfn) {
		char c;
		dr++;
		print_dir_record(dr);
		for(int j = 0x1; j < 0xb; j += 2) {
			c = ((uint8_t *)dr)[j];
			if(c == 0) return dr;
			printk("%c", c);
		}
		for(int j = 0xe; j < 0x1a; j += 2) {
			c = ((uint8_t *)dr)[j];
			if(c == 0) return dr;
			printk("%c", c);
		}
		for(int j = 0x1c; j < 0x1e; j += 2) {
			c = ((uint8_t *)dr)[j];
			if(c == 0) return dr;
			printk("%c", c);
		}
	} else {
		for(int c = 0; c < 11; ++c) { 
			if (dr->dir_name[c] == 0) { break; }
			printk("%c", dr->dir_name[c]); 
		}
		printk("\n");
		return dr + 1;
	}

	return dr;
}

uint32_t cluster_no_to_lba(uint32_t cluster_no) {
	return vol_data[chosen_partition].cluster_begin_lba + 
		(cluster_no - 2) * vol_data[chosen_partition].sectors_per_cluster;
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

void fat32_inspect_dir(uint32_t cluster_no) {
	if(!did_init) {
		panic("Attempted to inspect the filesystem before initialization.\n");
	}

	uint8_t *dst = (uint8_t *)FAT_end;
	uint8_t *saved_begin = dst;
	uint32_t bytes_per_cluster = 
		vol_data[chosen_partition].sectors_per_cluster * 
		all_vol_ids[chosen_partition].bytes_per_sector;

	printk("Getting the directory cluster.\n");
	do {
		read_cluster(cluster_no, dst);
		cluster_no = fat32_next_cluster(cluster_no);
		dst += bytes_per_cluster;
	} while(cluster_no != FAT32_TERMINAL_CLUSTER_NO);

	struct dir_record *dir_rec = (struct dir_record *)saved_begin;
	do {
		dir_rec = print_dir_record(dir_rec);
	} while(dir_rec != NULL);
}

int fat32_init(int (*read)(unsigned, unsigned char *, unsigned), int (*write)(const unsigned char *, unsigned int, unsigned int)) {
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
			} 			partitions_in_use |= 1<<i;

			(void)readsector(s->lba_begin, (unsigned char *)&all_vol_ids[i], 1);

			if((uint32_t)all_vol_ids[i].bytes_per_sector != SD_SECTOR_SIZE) {
				printk("bps: %d, sector size: %d\n", 
						all_vol_ids[i].bytes_per_sector, SD_SECTOR_SIZE);
				panic("Partition uses different sector size than sd driver.\n");
			}

			printk("+-----------------------\n");
			printk("|Fat32 parition [ %d ]\n", i);
			printk("|start lba: %d\n", boot_sector.partitions[i].lba_begin);
			printk("|num sectors: %d\n", boot_sector.partitions[i].num_sectors);
			fat32_dump_vol_id(&all_vol_ids[i]);

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

	printk("Reading the FAT into main memory...\n");
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
	printk("Done reading %d FAT sectors into memory.\n", i);

	did_init = 1;
	return 0;
}


// Find the first free cluster in the FAT table
uint32_t find_free_cluster() {
    for (uint32_t i = 2; i < ((FAT_end - FAT) / sizeof(uint32_t)); i++) {
        if (FAT[i] == 0) {
            return i;
        }
    }
    return 0; // No free clusters found
}


// Writes a cluster to the pi
void write_cluster(uint32_t cluster_no, uint8_t *src) {
    uint32_t lba = cluster_no_to_lba(cluster_no);
    printk("fat32: writing cluster %d (lba %d)\n", cluster_no, lba);
    for (int i = 0; i < all_vol_ids[chosen_partition].sectors_per_cluster; ++i) {
        writesector(src, lba++, 1);
        src += SD_SECTOR_SIZE;
    }
}


char *strcpy(char *dest, const char *src, size_t n) {
    size_t i;

    // Copy characters from src to dest until n characters are copied or
    // a null character is encountered in src
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }

    // If less than n characters were copied, pad dest with null characters
    for (; i < n; i++) {
        dest[i] = '\0';
    }

    return dest;
}

void create_file(const char *filename, uint8_t *data, uint32_t size) {
    if (!did_init) {
        panic("Attempted to write to filesystem before initialization.\n");
    }

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



