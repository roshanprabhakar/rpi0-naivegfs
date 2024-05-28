#include "rpi.h"
#include "fat32.h"

// Reads (sector lba, buffer dst, number of sectors)
static int (*readsector)(unsigned lba, unsigned char *buf, unsigned num);
static int did_init = 0;

int fat32_init(int (*read)(unsigned, unsigned char *, unsigned)) {
	readsector = read;
	did_init = 1;
	return 0;
}

int fat32_get_partition_info(void) {
	if(!did_init) {
		printk("fat32: partition requested without init\n");
		return -1;
	}

	struct mbr boot_sector;
	(void)readsector(MBR_SECTOR, (unsigned char *)&boot_sector, 1);
	if(boot_sector.magic1 != 0x55 || 
		 boot_sector.magic2 != 0xaa) {
		printk("fat32: detected corrupt boot sector\n");
		return -1;
	}
	
	printk("Fat32 partitions in use:\n");
	for(int i = 0; i < 4; ++i) {
		printk("[%d] start lba: %d, num sectors: %d\n",
				i+1, boot_sector.partitions[i].lba_begin,
				boot_sector.partitions[i].num_sectors);
	}

	return 0;
}

