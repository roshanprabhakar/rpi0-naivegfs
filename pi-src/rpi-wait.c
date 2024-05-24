#include "rpi.h"

void rpi_wait(void) { }

void delay_cycles(uint32_t ticks) {
	while(ticks-- > 0) {
		asm("add r1, r1, #0");
	}
}
