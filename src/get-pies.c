#include "get-pies.h"

#include <sys/stat.h>
#include <string.h>

#define ARRAY_LEN(arr) (sizeof(arr)/sizeof(arr[0]))


static const char *tty_prefixes[] = {
	"cu.SLAB_USB",
	"cu.usbserial",
};


int filter(const struct dirent *d) {
	for(unsigned i = 0; i < ARRAY_LEN(tty_prefixes); ++i) {
		char const *prefix = tty_prefixes[i];
		int prefix_len = strlen(prefix);

		if(strncmp(prefix, d->d_name, prefix_len) == 0) return 1;
	}
	return 0;
}


struct dirent **get_connected_pies(int *num_pies) {
	struct dirent **namelist;
	*num_pies = scandir("/dev", &namelist, filter, alphasort);
	return namelist;
}
