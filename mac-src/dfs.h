#ifndef _DFS_ROOT_DEF_
#define _DFS_ROOT_DEF_

struct __attribute__((packed)) dfs_entry {
	uint32_t cluster_no :28;
	uint32_t device_no 	:4;

	char name[11];
	uint8_t dir_attr;
};

struct dfs_file {
	uint32_t first_free_id;
	uint8_t pad[12];
	struct dfs_entry entries[31];
};

#endif // _DFS_ROOT_DEF_
