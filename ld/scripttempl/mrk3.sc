# Script for generating linker script for MRK3
# (This linker script is not parameratised at all)

cat <<EOF
OUTPUT_FORMAT("elf32-mrk3", "elf32-mrk3", "elf32-mrk3")
OUTPUT_ARCH(mrk3)

/* Set up some symbols to be used in building the origin addresses of
   memory regions.  The memory space id values used here must not
   change, they are known throughout the toolchain.*/

MRK3_MEM_SPACE_SYS  = 0x10000000;
MRK3_MEM_SPACE_USER = 0x20000000;
MRK3_MEM_SPACE_SSYS = 0x40000000;

MRK3_MEM_TYPE_DATA  = 0x00000000;
MRK3_MEM_TYPE_CODE  = 0x01000000;

ENTRY (_start)

MEMORY
{
   SSM_CODE (RX) : ORIGIN = MRK3_MEM_SPACE_SSYS + MRK3_MEM_TYPE_CODE + 0x0, LENGTH = 0x00FFFFFF
   SSM_DATA (RW) : ORIGIN = MRK3_MEM_SPACE_SSYS + MRK3_MEM_TYPE_DATA + 0x100, LENGTH = 0x00FFFEFF
}

SECTIONS
{
    /* .text in Super System code space. */
    .text :
    {
        KEEP (*(.vectors))
        KEEP (*(SORT(.text.compiler_rt.*)))
        *(.text .text.*)
    } >SSM_CODE

    /* .data in Super System data space.  */
    .data :
    {
        ___data_start = ADDR (.data);
        *(.data)
        *(.data.*)
        ___data_end = .;
    } >SSM_DATA

    /* .bss in Super System data space.  */
    .bss : ALIGN (0x10)
    {
        ___bss_start = ADDR (.bss);
        *(.bss)
        *(.bss.*)
        *(COMMON)
        ___bss_end = .;
    } >SSM_DATA

    /* .rodata in Super System data space.  */
    .rodata : ALIGN (0x10)
    {
        ___rodata_start = ADDR (.rodata);
        *(.rodata)
        *(.rodata.*)
        ___rodata_end = .;
    } >SSM_DATA

    PROVIDE (___heap_start = ALIGN (., 0x10));
}

PROVIDE (___STACK_END = (0xfff8));
PROVIDE (___heap_end = (___STACK_END + ___heap_start) / 2);
EOF
