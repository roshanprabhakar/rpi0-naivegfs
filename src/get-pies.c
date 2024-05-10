#include "libmac.h"

#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

static const char *tty_prefixes[] = {
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

void free_pie_list(struct dirent **l, int n) {
	for(int i = 0; i < n; ++i) free(l[i]);
	free(l);
}
