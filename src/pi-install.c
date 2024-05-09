/**
 * This program finds all devices connected via usb, assumes they
 * are pis, and initiates the boot process on all of them. The
 * boot protocol was developed as a cs240lx lab, and is documented 
 * here:
 *
 * https://github.com/dddrrreee/cs140e-24win/blob/main/labs/7-bootloader/BOOTLOADER.md
 **/

// For now, deplay the same program to all the pies.

#include "get-pies.h"

#include <stdio.h>
#include <stdlib.h>
#include <termios.h>

#define panic(str...) fprintf(stderr, str); exit(0)

static inline char const *get_arg(int *i, int argc, char const **argv) {
	if (*i >= argc) { panic("Not enough arguments!\n"); }
	return argv[(*i)++];
}

int main(int argc, char const **argv) {
	int num_pies;
	struct dirent **devs = get_connected_pies(&num_pies);	
	if(num_pies == -1) { panic("Could not retrieve pies!\n"); }


	unsigned baud_rate = B115200;
	unsigned boot_addr = 0x8000; // Boot address for the pi0s.

	int i = 1;
	char const *pi_prog = get_arg(&i, argc, argv);

	for(unsigned i = 0; i < num_pies; ++i) {
		printf("%s\n", devs[i]->d_name);
	}
	
	// free(dev_names);
	return 0;
}
