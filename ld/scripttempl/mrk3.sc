# Script for generating linker script for MRK3
# (This linker script is not parameratised at all)

cat <<EOF
OUTPUT_FORMAT("elf32-mrk3", "elf32-mrk3", "elf32-mrk3")
OUTPUT_ARCH(mrk3)

ENTRY (_start)

SECTIONS
{
	/* Code in Super System code space. */
	. = 0x000000;
	.text : AT ( 0x41000000 ) { *(.vectors)
	                            *(.text)
	                            *(SORT(.text.*)) }
	/* Data in Super System data space */
	PROVIDE (___data_start = 0x100);
	. = ___data_start;
	.data : AT ( 0x40000000 + ___data_start ) { *(.data) }
	. = ALIGN (., 0x10);
	PROVIDE (___bss_start = .);
	.bss  : AT ( 0x40000000 + ___bss_start ) { *(.bss) } 
	___bss_end = .;
	.rodata : AT ( 0x40000000 + ___bss_end ) { *(.rodata) }
	PROVIDE (___heap_start = ALIGN (., 0x10));
}

PROVIDE (___STACK_END = (0xfff8));
PROVIDE (___heap_end = (___STACK_END + ___heap_start) / 2);
EOF
