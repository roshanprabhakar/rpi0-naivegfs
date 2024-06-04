#include <stdint.h>

char *strncpy(char *dst, const char *src, uint32_t n) {
	if(n == 0) { return dst; }

	uint32_t i;

	// Copy characters from src to dest until n characters are copied or
	// a null character is encountered in src
	for(i = 0; i < n && src[i] != 0; i++) {
		dst[i] = src[i];
	}

	// Insert the terminator.
	if(i == n) {
		dst[n - 1] = 0;
	} else {
		dst[i] = 0;
	}

	return dst;
}

