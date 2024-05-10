#include "libmac.h"
#include "bootpi.h"

#include "boot-defs.h"
#include "boot-crc32.h"

#include <unistd.h>

void put8(int fd, uint8_t v) {
	int n = write(fd, &v, sizeof(v));
	if(n != sizeof(v)) {
		panic("put8: Expected a write of 1 byte, write returned %d\n", n);
	}
}

uint8_t get8(int fd) {
	uint8_t out;
	int n = read(fd, &out, 1);
	if(n < 0) { panic("get8: Read of 1 byte returned error=%d: disconnected?\n", n); }
	else if(n == 0) { panic("get8: Read returned 0 bytes. r/pi not responding [reboot?]\n"); }
	return out;
}

void put32(int fd, uint32_t v) {
	int n = write(fd, &v, sizeof(v));
	if(n != sizeof(v)) {
		panic("put32: Expected a write of %ld bytes, actual write was %d\n", sizeof(v), n);
	}
}

uint32_t get32(int fd) {
	return get8(fd)
		| get8(fd) << 8
		| get8(fd) << 16
		| get8(fd) << 24
		;
}

int boot(pi *p, void *program, int nbytes) {
	printf("Booting pi id<%d> %s..\n", p->fd, p->name);

	uint32_t op = get32(p->fd);

	// Drain until we see a GET_PROG_INFO.
	while(op != GET_PROG_INFO) {
		op = op >> 8 | (uint32_t)get8(p->fd) << 24;

		if(op == PRINT_STRING) {
			printf("pi sending string: ");
			uint32_t len = get32(p->fd);
			for(unsigned i = 0; i < len; ++i) {
				printf("%c", (char)get8(p->fd));
			}
			printf("\n");
		}
	}

	// Reply to the GET_PROG_INFO.
	put32(p->fd,PUT_PROG_INFO);
	put32(p->fd,ARMBASE);
	put32(p->fd,nbytes);
	put32(p->fd,crc32(program,nbytes));
		
	// Drain any extra GET_PROG_INFO
	while((op = get32(p->fd)) == GET_PROG_INFO)
		;

	// Exit condition means next request has been sent, or corrupt.
	// Deal with possible corrupt request here.
	if(op != GET_CODE) {
		panic("Did not receive a GET_CODE from pi<%d>, aborting boot.\n", p->fd);
		return -1;
	}

	// Send a PUT_CODE + the code.
	put32(p->fd,PUT_CODE);	
	for(unsigned i = 0; i < nbytes; ++i) {
		put8(p->fd,((uint8_t *)program)[i]);
	}

	// Wait for a BOOT_SUCCESS.
	unsigned i = 0;
	while((op = get32(p->fd)) != BOOT_SUCCESS) {
		if(i > (uint16_t)-1 << 16) {
			panic("Did not receive a BOOT_SUCCESS from pi<%d>, aborting boot.\n", p->fd);
			return -1;
		}
	}

	printf("pi<%d> successfully booted!\n", p->fd);
	return 0;
}
