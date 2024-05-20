@ Needed by some 240 binaries, not 100% sure what's going on
@ here.

.globl dev_barrier
.globl dmb

dev_barrier:
	b dsb

/* Data memory barrier. */
dmb:
	mov r0, #0
	mcr p15, 0, r0, c7, c10, 5
	bx lr

/* Used to be called "drain write buffer". 
 * Includes all cache operations is a suberset
 * of dmb. */

.globl dsb
dsb:
	mcr p15, 0, r0, c7, c10, 4
	bx lr

