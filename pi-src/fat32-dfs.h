#ifndef _FAT32_DFS_H_
#define _FAT32_DFS_H_

#include "fat32.h"
#include <stdint.h>

#define FAT32_DFS_CFG_FILENAME "F32-DFS CFG"
#define MAX_DFS_NAME_LEN 30

uint32_t dfs_init_config(void);

#endif // _FAT32_DFS_H_
