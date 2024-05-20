#ifndef _RPI_CONSTANTS_
#define _RPI_CONSTANTS_

#define STACK_ADDR          0x8000000
#define STACK_ADDR2         0x7000000

#define INT_STACK_ADDR      0x9000000
#define INT_STACK_ADDR2      0xA000000

#define FREE_MB            0x6000000

#define HIGHEST_USED_ADDR INT_STACK_ADDR

#define CYC_PER_USEC 700
#define PI_MHz (700*1000*1000UL)

/* from A2-2 */
#define USER_MODE       0b10000
#define FIQ_MODE        0b10001
#define IRQ_MODE        0b10010
#define SUPER_MODE      0b10011
#define ABORT_MODE      0b10111
#define UNDEF_MODE      0b11011
#define SYS_MODE        0b11111


// 1.4285714285714286 nanosecond per cycle: we don't have fp so
// use 142857 and then divide by 100000
// 
#define cycles_to_nanosec(c) (((c) * 142857UL) / 100000UL)

// if we overclock, will have to change this stuff.
#define usec_to_cycles(usec) ((usec) * CYC_PER_USEC)

#endif
