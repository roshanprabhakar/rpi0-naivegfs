#ifndef _RPI_H_
#define _RPI_H_

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/* From start.S */

void put32(int addr, int v);
int get32(int addr);

void BRANCHTO(void (*fn)(void));

/* uart routines. */

void uart_init(void);
void uart_disable(void);
int uart_get8(void);
int uart_put8(uint8_t c);
int uart_hex(unsigned h);
int uart_get8_async(void);
int uart_can_put8(void);
int uart_can_putc(void);
void uart_flush_tx(void);

/* Output routines. */

// Emit a single string.
int putk(const char *msg);

// printf with a lot of restrictions.
int printk(const char *fmt, ...);

// vprintf with a lot of restrictions.
int vprintk(const char *fmt, va_list ap);

// print string to <buf>
#include <stdarg.h>
int snprintk(char *buf, unsigned buflen, const char *fmt, ...);
int vsnprintk(char *buf, unsigned buflen, const char *fmt, 
		va_list ap);

/* Other */
void rpi_wait(void);

#endif
