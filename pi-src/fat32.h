#ifndef _FAT32_H_
#define _FAT32_H_

#define SD_SECTOR_SIZE 512
#define MBR_SECTOR 0

int fat32_init(int (*)(unsigned, unsigned char *, unsigned));
int fat32_get_partition_info(void);

struct __attribute__((packed)) partition {
	uint8_t boot_flag;
	uint8_t chs_begin[3];
	uint8_t type_code;
	uint8_t chs_end[3];
	uint32_t lba_begin;
	uint32_t num_sectors;
};

struct mbr {
	uint8_t boot_code[446];
	struct partition partitions[4];
	uint8_t magic1;
	uint8_t magic2;
};

#endif // _FAT32_H_
