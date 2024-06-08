#ifndef _FAT32_DFS_H_
#define _FAT32_DFS_H_

#include "fat32.h"
#include <stdint.h>

#define FAT32_DFS_CFG_FILENAME "FS32-DFSCFG"
#define MAX_DFS_NAME_LEN 30

uint32_t dfs_init_config(void);

struct config_sector {
	struct __attribute__((packed)) {
		char dfs_id[30];
		uint16_t id;
	} entries[16];
};

#endif // _FAT32_DFS_H_
