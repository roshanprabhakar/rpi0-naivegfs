/**
 * This program finds all devices connected via usb, assumes they
 * are pis, and initiates the boot process on all of them. The
 * boot protocol was developed as a cs240lx lab, and is documented 
 * here:
 *
 * https://github.com/dddrrreee/cs140e-24win/blob/main/labs/7-bootloader/BOOTLOADER.md
 **/

// For now, deplay the same program to all the pies.

// #include "get-pies.h"

#include <stdio.h>

#define panic(str...) fprintf(stderr, str)

static inline char const *get_arg(int *i, int argc, char const **argv) {
	if (*i >= argc) { panic("Not enough arguments!\n"); return NULL; }
	return argv[(*i)++];
}

int main(int argc, char const **argv) {
	// char **dev_names = get_connected_pies();	
	char const *pi_prog = 0;

	unsigned baud_rate = B115200;
	unsigned boot_addr = 0x8000; // Boot address for the pi0s.

	int i = 1;
	char const *pi_prog = get_arg(&i, argv);


fail:
	free(dev_names);
	return 0;
}
