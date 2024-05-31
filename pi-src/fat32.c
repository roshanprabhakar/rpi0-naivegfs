#include "rpi.h"
#include "fat32.h"

// Reads (sector lba, buffer dst, number of sectors)
static int (*readsector)(unsigned lba, unsigned char *buf, unsigned num);
static int did_init = 0;
static uint8_t partitions_in_use = 0;

static struct mbr boot_sector;
static struct volume_id all_vol_ids[4];
static struct fat_volume_data vol_data[4];

static void fat32_dump_vol_id(struct volume_id *vol_id) {
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

static int fat32_dump_dir_recs(struct dir_sector *ds) {
	struct dir_record *d;
	for(int i = 0; i < sizeof(*ds)/sizeof(ds->records[0]); ++i) {
		d = &(ds->records[i]);
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

// Sequential traversal is incorrect, change to use the fat.
static void inspect_root(int partition_no) {
	uint32_t root_lba = vol_data[partition_no].cluster_begin_lba;
	struct dir_sector ds;
	for(uint32_t i = root_lba;; ++i) {
		(void)readsector(i, (unsigned char *)&ds, 1);
		int contains_terminal = fat32_dump_dir_recs(&ds);
		if(contains_terminal) break;
	}
}

int fat32_get_info(void) {
	if(!did_init) panic("Using fat32 without initialization");
	return 0;
}

int fat32_init(int (*read)(unsigned, unsigned char *, unsigned)) {
	readsector = read;
	(void)readsector(MBR_SECTOR, (unsigned char *)&boot_sector, 1);

	for(int i = 0; i < 4; ++i) {
		struct partition *s = &boot_sector.partitions[i];
		if(s->num_sectors != 0) {
			partitions_in_use |= 1<<i;
			(void)readsector(s->lba_begin, (unsigned char *)&all_vol_ids[i], 1);

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

			inspect_root(i);
		}
	}

	did_init = 1;
	return 0;
}
