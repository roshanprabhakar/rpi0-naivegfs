#ifndef __GETCODE_H__
#define __GETCODE_H__

#include "rpi.h"

// return a 32-bit: little endien order.
//
// NOTE: as-written, the code will loop forever if data does
// not show up or the hardware dropped bytes (e.g. b/c 
// you didn't read soon enough)
//
// After you get the simple version working, you should fix 
// it by making a timeout version.
static inline uint32_t boot_get32(void) {
	uint32_t u = uart_get8();
		u |= uart_get8() << 8;
		u |= uart_get8() << 16;
		u |= uart_get8() << 24;
	return u;
}

// send 32-bits on network connection.  (possibly let the
// network implementation override for efficiency)
static inline void boot_put32(uint32_t u) {
	uart_put8((u >> 0)  & 0xff);
	uart_put8((u >> 8)  & 0xff);
	uart_put8((u >> 16) & 0xff);
	uart_put8((u >> 24) & 0xff);
}

#endif

