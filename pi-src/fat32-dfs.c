#include "rpi.h"
#include "fat32-dfs.h"

uint32_t dfs_init_config() {
	uint32_t config_cluster_no = 
		fat32_get_cluster_no(FAT32_DFS_CFG_FILENAME);

	if(config_cluster_no == -1) {

		return fat32_alloc_local_file(2, FAT32_DFS_CFG_FILENAME);
		
	}	else { return config_cluster_no; }
}


