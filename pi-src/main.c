#include "rpi.h"
#include "bzt-sd.h"
#include "fat32.h"
#include "fat32-dfs.h"
#include "boot-defs.h"
#include "get-code.h"

// Defined by the linker. They only have addresses, not
// values.
extern int bss_start;
extern int bss_end;

// Called to setup the C runtime and initialize commonly used 
// subsystems.
void _cstart() {
	void entry(void);

	// Zero out the bss.
	memset(&bss_start,0,(uintptr_t)&bss_end - (uintptr_t)&bss_start);

	// DO NOT manually call uart_init, the bootloader already does this.
	// // // DO NOT DO THIS // // // uart_init();

	// Initialize the SD card.
	sd_init();

	// Initialize the fat32 system on the SD card.
	// fat32_init(sd_readblock, sd_writeblock);	
	
	// Jump to main app-level code.
	entry();
}

void uart_put_dummy() {
	uart_put8(0x78);
	uart_put8(0x56);
	uart_put8(0x34);
	uart_put8(0x12);
}

void uart_put_dummy2() {
	uart_put8(0xff);
	uart_put8(0xde);
	uart_put8(0xbc);
	uart_put8(0x9a);
}

void get_string(char *name) {
	uint32_t op = 0;

	for(int i = 0; i < MAX_DFS_NAME_LEN / 4; ++i) {

		for(int j = 0; j < 4; ++j) {
			op = op >> 8 | (uint32_t)uart_get8() << 24;
		}

		((uint32_t *)name)[i] = op;

		uart_put_dummy();

		if(op == 0xffffffff) break;
	}
}

int id_for_dfs(
		uint32_t cfg_cluster_no, char *dfs_name, int id_if_unassigned
	) {

#if 1
	struct config_sector s;
	sd_readblock(cluster_no_to_lba(cfg_cluster_no), 
			(unsigned char *)&s, 1);

#if 0
	printk("config sector before:\n");
	for(int i = 0; i < sizeof(s.entries)/sizeof(s.entries[0]); ++i) {
		if(s.entries[i].dfs_id[0] == 0) {
			printk("End reached!\n");
			break;
		} else {
			for(int j = 0; j < 30; ++j) {
				printk("%c", s.entries[i].dfs_id[j]);
			}
			printk("\tid: %d\n", s.entries[i].id);
		}
	}
#endif

	for(int i = 0; i < sizeof(s.entries)/sizeof(s.entries[0]); ++i) {
		if(s.entries[i].dfs_id[0] == 0) {
			
			if(id_if_unassigned == -1) {
				_halt();
			} else {
				strncpy(s.entries[i].dfs_id, dfs_name, MAX_DFS_NAME_LEN);
				s.entries[i].id = id_if_unassigned;
				break;
			}

		} else {
			if(strncmp(s.entries[i].dfs_id, dfs_name, MAX_DFS_NAME_LEN)
					== 0) {
				return (int)s.entries[i].id;
			}
		}
	}

	sd_writeblock((unsigned char *)&s, 
			cluster_no_to_lba(cfg_cluster_no), 1);

#if 0
	printk("config sector after:\n");
	for(int i = 0; i < sizeof(s.entries)/sizeof(s.entries[0]); ++i) {
		if(s.entries[i].dfs_id[0] == 0) {
			printk("End reached!\n");
			break;
		} else {
			for(int j = 0; j < 30; ++j) {
				printk("%c", s.entries[i].dfs_id[j]);
			}
			printk("\tid: %d\n", s.entries[i].id);
		}
	}
#endif

	return -1;
#else
	static struct __attribute__((packed)) {
		uint8_t buf[512];
	} blank_sector = {0};

	sd_writeblock((unsigned char *)&blank_sector,
			cluster_no_to_lba(cfg_cluster_no), 1);
	return 0;
#endif
}


// All application level code must get thrown into here.
void entry() {

	uint32_t op = 0;

	// Wait for sending dfs id signal.
	while(op != FS_ID_START) {
		op = op >> 8 | (uint32_t)uart_get8() << 24;
	}
	// printk("pi: receiving dfs id.\n");

	uart_put_dummy();

	char dfs_name[MAX_DFS_NAME_LEN];
	get_string(dfs_name);

	// Wait for send config status.
	while(op != CONFIG_INIT) {
		op = op >> 8 | (uint32_t)uart_get8() << 24;
	}	
	// printk("Received boot dfs command,\n");

	fat32_init(sd_readblock, sd_writeblock);	
	uint32_t cfg_file_cluster_no = dfs_init_config();

	// Dummy here indicates that the pi has read its fat and located
	// its config file.
	uart_put_dummy();

	// Now, wait for the command to begin searching for our id number.

	op = 0;
	while(op != REQUEST_ID) {
		op = op >> 8 | (uint32_t)uart_get8() << 24;
	}
	
	uart_put_dummy();

	// The next word will be \xff\xff\xff\xN
	// where N is the id of the pi if it does not already have one.

	for(int i = 0; i < 4; ++i) {
		op = op >> 8 | (uint32_t)uart_get8() << 24;
	}
	op &= 0xff;

	int id = id_for_dfs(cfg_file_cluster_no, dfs_name, op);
	if(id == -1) {
		uart_put_dummy2(); // assignable id used.

		uart_put8(op & 0xff);
		uart_put8(op & 0xff);
		uart_put8(op & 0xff);
		uart_put8(op & 0xff);

	} else {
		uart_put_dummy(); // assignable id not used.

		uart_put8(id);
		uart_put8(id);
		uart_put8(id);
		uart_put8(id);
	}

	// Send the id number the pi claims to have.


	putk("DONE!!!\n");
	return;
	
#if 0
	printk("Booting dfs: ");
	for(int i = 0; i < MAX_DFS_NAME_LEN; ++i) {
		if(dfs_name[i] == 0xff) break;
		printk("%c", dfs_name[i]);
	}
	printk("\n");

	printk("Config file lives at cluster %d.\n", 
			cfg_file_cluster_no);

	int id = id_for_dfs(cfg_file_cluster_no, dfs_name, -1);
	if(id == -1) {
		printk("Code shouldn't have reached here.\n");
	}
#endif
	

#if 0
	// First: Do we have a rpi-dfs-meta file? If so, read it into memory.
	// The first 8 bits contain the device id. If this file does not exist,
	// request an id from the mac, informing the mac that we would like to
	// be added to the dfs.
	//
	uint32_t cfgf_cluster_no = fat32_find_cluster_no(
			"RPI-DFS-CFG", FAT32_ROOT_CLUSTER_NO);

	if(cfgf_cluster_no == -1) {
		printk("This pi is not part of the dfs. Requesting an id...\n");

	}

	// fat32_inspect_dir(FAT32_ROOT_CLUSTER_NO);
	// uint32_t start = fat32_alloc_local_file(5, "new file");
	// printk("======================================================\n");
	// printk("Allocd file at %d\n", start);
	// printk("======================================================\n");
	// fat32_inspect_dir(FAT32_ROOT_CLUSTER_NO);

	char const *filename = "CONFIG  TXT";
	uint32_t start_cluster_no = 
		fat32_find_cluster_no(filename, FAT32_ROOT_CLUSTER_NO);
	printk("start cluster no: %d\n", start_cluster_no);

	// Close the filesystem.
	fat32_shutdown();	
#endif

	putk("DONE!!!\n");
	return;
}
