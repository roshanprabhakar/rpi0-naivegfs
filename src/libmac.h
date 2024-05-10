#ifndef _LIB_MAC_
#define _LIB_MAC_

#include <stdio.h>
#include <dirent.h>

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))

#define panic(str...) fprintf(stderr, str)

struct dirent **get_connected_pies(int *);
void free_pie_list(struct dirent **, int);

void *get_bytes(char const *file, int *);

int set_tty_to_8n1(int, unsigned, double);

#endif // _LIB_MAC_
