#include "libmac.h"
#include "bootpi.h"

#include "boot-defs.h"
#include "boot-crc32.h"

#include <unistd.h>
#include <time.h>

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
	struct timespec req = {0};
	req.tv_nsec = 50000000L;
	nanosleep(&req, (struct timespec *)NULL);
}

uint32_t get32(int fd) {
	return get8(fd)
		| get8(fd) << 8
		| get8(fd) << 16
		| get8(fd) << 24
		;
}

void send_string(pi *p, char *to_send, int n) {
	uint32_t op;
	for(int i = 0; i < n; ++i) {
		put32(p->fd, ((uint32_t *)to_send)[i]);

		while((op = get32(p->fd)) != DUMMY)
			;
	}
}

int dfs_boot(
		pi *p, char *dfs_name, int words_in_name, uint32_t *assignable_id
	) {

	uint32_t op = 0;

	// Indicate that the next n <= 30 chars will be the dfs name.
	put32(p->fd,FS_ID_START);

	// Wait for dummy.
	while((op = get32(p->fd)) != DUMMY)
		;

	// Send the name of the dfs to locate.
	send_string(p, dfs_name, words_in_name);

	// Send init signal, instructing the pi to identify the dfs config file
	// and initialize its fat32 system.
	put32(p->fd,CONFIG_INIT);

	struct timeval wait_timeout = {
		.tv_sec = 20,
		.tv_usec = 0
	};

	fd_set wait;
	FD_ZERO(&wait);
	FD_SET(p->fd,&wait);

	printf("Waiting for pi<%d> to set up its fat32 subsystem.\n", p->fd);
	int num_ready = select(p->fd + 1, &wait, NULL, NULL, &wait_timeout);

	if(num_ready == 0) {
		printf("pi<%d> did not set up its fat32 in time, moving on.\n", p->fd);
		return -1;
	}

	// Now that data is available, wait to see the dummy indicating
	// completion.

	while((op = get32(p->fd)) != DUMMY)
		;
	printf("Pi has set up its fat.\n");

	// Now, ask the pi for its id. It can do this since it has already
	// received the dfs name.
	
	put32(p->fd, REQUEST_ID);

	while((op = get32(p->fd)) != DUMMY)
		;

	put32(p->fd, (0xffffff00 | *assignable_id));

	// Now, as implemented, if the assignable number is -1, then the 
	// pi will hang. If this is the case, we want to return. Otherwise,
	// wait for a assign accepted, in which case we increment the 
	// assignable id, or a assign rejected, in which case we do nothing.
	
	do {
		op = get32(p->fd);
		if(op == DUMMY || op == DUMMY2) { break; }
	} while(1);


	if(op == DUMMY2) {
		printf("pi<%d> took id %d.\n", p->fd, *assignable_id);
		(*assignable_id)++;
	}

	op = get32(p->fd);
	printf("pi<%d> claims to have id %d.\n", p->fd, op & 0xff);

	return 0;
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
		++i;
	}

	printf("pi<%d> successfully booted!\n", p->fd);
	p->open = 1;

	return 0;
}


