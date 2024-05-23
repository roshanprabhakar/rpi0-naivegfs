#include "rpi.h"
#include "sd.h"

// Defined by the linker. They only have addresses, not
// values.
extern int bss_start;
extern int bss_end;

// Expects a len divisible by word size (sizeof(uint32_t))
void *memset(void *ptr, uint32_t b, uint32_t len) {
	uint32_t buf;
	for(uint32_t i = 0; i < sizeof(uint32_t); ++i) {
		((uint8_t *)&buf)[i] = (uint8_t)b;
	}

	for(uint32_t i = 0; i < len / sizeof(uint32_t); ++i) {
		((uint32_t *)ptr)[i] = buf;
	}

	for(uint32_t i = len / sizeof(uint32_t) * sizeof(uint32_t);
			i < len; ++i) {
		((uint8_t *)ptr)[i] = (uint8_t)b;
	}
	return ptr;
}

// Called to setup the C runtime and initialize commonly used 
// subsystems.
void _cstart() {
	void entry(void);

	// Zero out the bss.
	memset(&bss_start,0,(uintptr_t)&bss_end - (uintptr_t)&bss_start);

	// Initialize hardware uart.
	uart_init();
	
	// Jump to main app-level code.
	entry();
}


// All application level code must get thrown into here.
void entry() {
	putk("Testing SD driver.\n");

	uint8_t to_write[512 * 2];
	for(int i = 0; i < 512 * 2 / 4; ++i) {
		((uint32_t *)to_write)[i] = 0xdeadbeef;
	}

	do_data_command(1, (uint32_t *)to_write, 78125000);

	putk("DONE!!!\n");
	return;
}
