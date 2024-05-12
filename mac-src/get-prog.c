#include "libmac.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void *get_bytes(char const *filename, int *size) {
	struct stat s;
	if(stat(filename,&s) == -1) return NULL;

	int fd = open(filename, O_RDONLY);
	if(fd == -1) return NULL;

	void *ret = malloc(s.st_size);

	int got;
	if((got = read(fd, ret, s.st_size)) != s.st_size) {
		free(ret);
		return NULL;
	}

	*size = (int)s.st_size;
	return ret;
}
