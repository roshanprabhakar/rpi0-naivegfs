#include <stdint.h>

char *strncpy(char *dst, const char *src, uint32_t n) {
	if(n == 0) { return dst; }

	uint32_t i;

	// Copy characters from src to dest until n characters are copied or
	// a null character is encountered in src
	for(i = 0; i < n && src[i] != 0 && src[i] != 0xff; i++) {
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

int strncmp(char const *str1, char const *str2, uint32_t n) {
	int ret = 0;
	for(uint32_t i = 0; i < n; ++i) {
		if(str1[i] == 0 || str2[i] == 0 || 
			 str1[i] == 0xff || str2[i] == 0xff) break;
		if(str1[i] != str2[i]) { ret = -1; break; }
	}
	return ret;
}
