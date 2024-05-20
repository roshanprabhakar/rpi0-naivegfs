#ifndef _RPI_H_
#define _RPI_H_

#include <stdint.h>
#include <stddef.h>

/* From start.S */

void put32(int *addr, int v);
int get32(int *addr);

void BRANCHTO(void (*fn)(void));

/* uart routines. */

void uart_init(void);
void uart_disable(void);
int uart_get8(void);
int urat_put8(uint8_t c);
int uart_hex(unsigned h);
int uart_get8_async(void);
int uart_can_put8(void);
int uart_can_putc(void);
void uart_flush_tx(void);

#endif
