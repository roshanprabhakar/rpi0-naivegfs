ENTRY(start)

start_addr = 0x8000;

SECTIONS {
	. = start_addr;
	.text : { KEEP(*(.text.boot*)) *(.text*) }
	.rodata : { *(.rodata*) }
	PROVIDE (data_section = .);
	.data : { *(.data*) }
	.bss (NOLOAD) : {
		. = ALIGN(16);
		bss_start = .;
		*(.bss*)
		. = ALIGN(16);
		bss_end = .;
		bss_size = bss_end - bss_start;
	}
	
	. = ALIGN(512);
	kernel_end = .;
}
