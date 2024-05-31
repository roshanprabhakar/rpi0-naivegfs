#include "rpi-asm.h"

.section ".text.boot"

.globl _start
_start:
	
	@ force the mode to be SUPER
	mov r0, #SUPER_MODE
	orr r0,r0,#(1<<7) @ disable interrupts
	msr cpsr, r0
	prefetch_flush(r1)

	mov sp, #STACK_ADDR		@ initialize stack pointer.
	mov fp, #0						@ clear frame pointer reg. don't think needed.
	@ bl notmain
	
	bl _cstart 						@ call our code to do initialization.
	b _halt								@ if they return, just hang.

.globl _halt
_halt:
	nop
	b _halt

@ Keep put32/get32 here so that they are in the same location in every binary.
@ If you want to override them easily, can re-defined the name in rpi-asm.h
@ 
@ We write them as assembly routines to make absolutely sure the compiler
@ cannot optimize them (reorder, eliminate, cache, etc.).

@
@ put32(addr,val), c-code equivalent;
@		- put32(addr,val) { *(uint32_t *)addr = val; }
@
@
.globl put32
.globl PUT32
PUT32:
put32:
	str r1,[r0]
	bx lr

@ get32(addr)
@ c-code equivalend;
@ 		get32(addr) { return *(uint32_t *)addr; }
@

.globl get32
.globl GET32
GET32:
get32:
	ldr r0, [r0]
	bx lr

@ Could be useful, jump to the address stored in r0. This doesn't mess with the link
@ register, so the called routine will return to our caller.
.globl BRANCHTO
BRANCHTO:
	bx r0
