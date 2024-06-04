#include "rpi.h"
#include "bzt-sd.h"
#include "fat32.h"

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

	// Initialize hardware uart.
	uart_init();

	// Initialize the SD card.
	sd_init();

	// Initialize the fat32 system on the SD card.
	fat32_init(sd_readblock, sd_writeblock);	
	
	// Jump to main app-level code.
	entry();
}


// All application level code must get thrown into here.
void entry() {

	// First: Do we have a rpi-dfs-meta file? If so, read it into memory.
	// The first 8 bits contain the device id. If this file does not exist,
	// request an id from the mac, informing the mac that we would like to
	// be added to the dfs.
	//

	fat32_inspect_dir(FAT32_ROOT_CLUSTER_NO);
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

	putk("DONE!!!\n");
	return;
}
