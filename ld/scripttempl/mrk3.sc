# Script for generating linker script for MRK3
# (This linker script is not parameratised at all)

cat <<EOF
OUTPUT_FORMAT("elf32-mrk3", "elf32-mrk3", "elf32-mrk3")
OUTPUT_ARCH(mrk3)

ENTRY (_start)

PROVIDE (___bss_start = __bss_start);
PROVIDE (___heap_start = end);
PROVIDE (___heap_end = (0x1500));
PROVIDE (___STACK_END = (0x2000));

SECTIONS
{
/* Code in System code space. */
. = 0x000000;
.text : AT ( 0x11000000 ) { *(.vectors)
                            *(.text) }
/* Data in System data space */
. = 0x00004000;
.data : AT ( 0x10004000 ) { *(.data) }
.bss  : AT ( 0x10004000 + SIZEOF (.data) ) { *(.bss) } 
}
EOF