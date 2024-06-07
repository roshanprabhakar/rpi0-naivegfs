#ifndef _BOOT_PI_
#define _BOOT_PI_

#include <stdint.h>

typedef struct {
	int open;
	int fd;
	char const *name;
} pi;

void put8(int, uint8_t);
uint8_t get8(int);

void put32(int, uint32_t);
uint32_t get32(int);

int boot(pi *, void *, int);
int dfs_boot(pi *);

#endif // _BOOT_PI_
