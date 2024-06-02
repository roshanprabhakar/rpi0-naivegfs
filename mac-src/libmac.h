#ifndef _LIB_MAC_
#define _LIB_MAC_

#include <stdint.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>

#include "bootpi.h"

/**
 * Collects all project-wide useful macros + functions that
 * occupy entire files (functions called from the client).
 **/

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#define panic(fmt, ...) { \
	fprintf(stderr, fmt,##__VA_ARGS__); \
	exit(-1); \
}

struct dirent **get_connected_pies(int *);
void free_pie_list(struct dirent **, int);

void *get_bytes(char const *file, int *);

int set_tty_to_8n1(int, unsigned, double);

void get_pi_output(pi *, int, int);

#endif // _LIB_MAC_
