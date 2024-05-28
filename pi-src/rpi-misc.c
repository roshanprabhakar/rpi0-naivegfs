
// Just throw misc routines into this file if there's no other
// good place for them.

#include <stdint.h>

void rpi_wait(void) { }

void delay_cycles(uint32_t ticks) {
	while(ticks-- > 0) {
		asm("add r1, r1, #0");
	}
}

int ignore(const char *fmt, ...) { return 0; }
