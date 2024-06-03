#include "rpi.h"
#include "fat32.h"

// Reads (sector lba, buffer dst, number of sectors)
static int (*readsector)(unsigned lba, unsigned char *buf, unsigned num);
static int did_init = 0;
static uint8_t partitions_in_use = 0;
static int chosen_partition;

static struct mbr boot_sector;
static struct volume_id all_vol_ids[4];
static struct fat_volume_data vol_data[4];

extern int kernel_end; 
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
		wchar_t c;
		dr++;
		print_dir_record(dr);

#if 0
		for(int j = 0x1; j < 0xb; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
		for(int j = 0xe; j < 0x1a; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
		for(int j = 0x1c; j < 0x1e; j += 2) {
			c = (wchar_t)(((uint16_t *)dr)[j/2]);
			if(c == 0) return dr;
			if(c != '\n') putwchar(c);
		}
#endif
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

	printk("wide char size: %d\n", sizeof(wchar_t));

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

int fat32_init(int (*read)(unsigned, unsigned char *, unsigned)) {
	readsector = read;
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
