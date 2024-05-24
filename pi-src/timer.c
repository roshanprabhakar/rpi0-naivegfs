#include "rpi.h"

uint32_t timer_get_usec_raw(void) {
	return get32(0x20003004);
}

void delay_us(uint32_t us) {
	uint32_t s = timer_get_usec_raw();
	while(1) {
		uint32_t e = timer_get_usec_raw();
		if((e - s) > us) return;
	}
}

void delay_ms(uint32_t ms) {
	delay_us(ms*1000);
}

void delay_sec(uint32_t sec) {
	delay_ms(sec*1000);
}
