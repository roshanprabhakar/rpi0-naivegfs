#include "rpi.h"

void panic(const char *msg) {
	putk("PANIC: ");
	putk(msg);
	putk("DONE!!!\n");
	_halt();
}
