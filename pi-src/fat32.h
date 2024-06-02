#ifndef _FAT32_H_
#define _FAT32_H_

#define SD_SECTOR_SIZE 512
#define MBR_SECTOR 0

#define FAT32_ROOT_CLUSTER_NO 2

#define FAT32_TERMINAL_CLUSTER_NO (((uint32_t)-1)>>4)

int fat32_init(int (*)(unsigned, unsigned char *, unsigned));
int fat32_get_info(void);

struct __attribute__((packed)) partition {
	uint8_t boot_flag;
	uint8_t chs_begin[3];
	uint8_t type_code;
	uint8_t chs_end[3];
	uint32_t lba_begin;
	uint32_t num_sectors;
};

struct __attribute__((packed)) mbr {
	uint8_t boot_code[446];
	struct partition partitions[4];
	uint8_t magic1;
	uint8_t magic2;
};

struct __attribute__((packed)) volume_id {
	uint8_t pad[0x0b];
	uint16_t bytes_per_sector;
	uint8_t sectors_per_cluster;
	uint16_t num_reserved_sectors;
	uint8_t num_fats;
	uint8_t pad2[0x13];
	uint32_t sectors_per_fat;
	uint32_t pad4;
	uint32_t root_dir_first_cluster;
	uint8_t pad5[0x1ce];
	uint8_t magic1;
	uint8_t magic2;
};

struct fat_volume_data {
	uint32_t fat_begin_lba;
	uint32_t cluster_begin_lba;
	uint8_t sectors_per_cluster;
	uint32_t root_dir_first_cluster;
};


// Invoked on instances of (struct dir_record).dir_attr
#define IS_RO(attr) (((attr) & (1<<0)) != 0)
#define IS_HIDDEN(attr) (((attr) & (1<<1)) != 0)
#define IS_SYSTEM(attr) (((attr) & (1<<2)) != 0)
#define IS_VOLID(attr) (((attr) & (1<<3)) != 0)
#define IS_DIR(attr) (((attr) & (1<<4)) != 0)
#define IS_ARCHIVE(attr) (((attr) & (1<<5)) != 0)

#define IS_LFN(attr) ((attr) == 0x0f)
#define LFN_IS_LAST_ENT(attr) ((attr) & (1<<6))

// Invoked on instances of struct dir_record
#define IS_TERMINAL(dr) ((dr).dir_name[0] == 0)

struct __attribute__((packed)) dir_record {
	unsigned char dir_name[11];
	uint8_t dir_attr;
	uint8_t pad1[8];
	uint16_t dir_first_cluster_high;
	uint8_t pad2[4];
	uint16_t dir_first_cluster_low;
	uint32_t dir_file_size;
};

typedef struct dir_record dir_sector[16];

void fat32_inspect_dir(uint32_t cluster_no);

#endif // _FAT32_H_
