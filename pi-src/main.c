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

	fat32_inspect_dir(FAT32_ROOT_CLUSTER_NO);

	putk("All SD writs done, it is safe to reset the pi.\n");
	putk("DONE!!!\n");
	return;
}
