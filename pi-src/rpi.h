#ifndef _RPI_H_
#define _RPI_H_

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>


/* Error handling */
void panic(const char *msg);

/* Timer functions */
void delay_cycles(uint32_t);

uint32_t timer_get_usec_raw(void);
void delay_us(uint32_t);
void delay_ms(uint32_t);
void delay_sec(uint32_t);

/* From start.S */

void put32(int addr, int v);
int get32(int addr);

void _halt(void);

void BRANCHTO(void (*fn)(void));


// initialize [XXX: we should take a baud rate?]
void uart_init(void);
// disable
void uart_disable(void);

// get one byte from the uart
int uart_get8(void);
// put one byte on the uart:
// returns < 0 on error.
int uart_put8(uint8_t c);

int uart_hex(unsigned h);

// returns -1 if no byte, the value otherwise.
int uart_get8_async(void);

// 0 = no data, 1 = at least one byte
int uart_has_data(void);

// 0 = no space, 1 = space for at least 1 byte
int uart_can_put8(void);
int uart_can_putc(void);

// flush out the tx fifo
void uart_flush_tx(void);


/* Output routines. */

// Ignore the string (useful for replacing output
// routines for drop-in code).
int ignore(const char *fmt, ...);

// Emit a single string.
int putk(const char *msg);

// printf with a lot of restrictions.
int printk(const char *fmt, ...);

// vprintf with a lot of restrictions.
int vprintk(const char *fmt, va_list ap);

// Needed to print utf16 characters.
int putwchar(wchar_t c);

// print string to <buf>
#include <stdarg.h>
int snprintk(char *buf, unsigned buflen, const char *fmt, ...);
int vsnprintk(char *buf, unsigned buflen, const char *fmt, 
		va_list ap);

/* Other */
void rpi_wait(void);
void *memset(void *, uint32_t, uint32_t);

char *strncpy(char *, const char *, uint32_t);
int strncmp(char const *, char const *, uint32_t);

#endif
