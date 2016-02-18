# Script for generating linker script for MRK3
# (This linker script is not parameratised at all)

cat <<EOF
OUTPUT_FORMAT("elf64-mrk3", "elf64-mrk3", "elf64-mrk3")
OUTPUT_ARCH(mrk3)

ENTRY (_start)

/* Logical memory regions are defined for supersystem mode, system mode card A
   process 1 and user mode card A process 1.  These are specified in the top 32
   bits of each address as follows

     MCPP000Txxxxxxxx

     M  - mode (0 = none, 1 = SSM, 2 = SM, 3 = UM)
     C  - card (0 = none, 1 = A, 2 = B, ..., 15 = O)
     PP - process (0 = none, all other value process ID <= 65,535)
     F  - type flag (0 = none, 1 = data, 2 = code)

   Within data regions, it is assumed writable data goes at 0x100 and read only
   data at 0x4000.
*/

MEMORY
{
    SSM_CODE (RX)        : ORIGIN = 0x1000000200000000, LENGTH = 0x1000000
    SSM_BASIC_RAM (W)    : ORIGIN = 0x1000000100000100, LENGTH = 0x0f00
    SSM_BASIC_CONST (R)  : ORIGIN = 0x1000000100004000, LENGTH = 0x0400

    SMA1_CODE (RX)       : ORIGIN = 0x2101000200000000, LENGTH = 0x1000000
    SMA1_BASIC_RAM (W)   : ORIGIN = 0x2101000100000100, LENGTH = 0x0f00
    SMA1_BASIC_CONST (R) : ORIGIN = 0x2101000100004000, LENGTH = 0x0400

    UMA1_CODE (RX)       : ORIGIN = 0x3101000200000000, LENGTH = 0x1000000
    UMA1_BASIC_RAM (W)   : ORIGIN = 0x3101000100000100, LENGTH = 0x0f00
    UMA1_BASIC_CONST (R) : ORIGIN = 0x3101000100004000, LENGTH = 0x0400
}

/* This default script puts everything in supersystem mode. */

SECTIONS
{
    /* .text in Super System code space. */
    .text :
    {
        KEEP (*(.vectors))
        KEEP (*(SORT(.text.compiler_rt.*)))
        KEEP (*(SORT(.text.libc.string.memcpy*)))
        KEEP (*(SORT(.text.libc.string.memset*)))
        KEEP (*(SORT(.text.libc.strtol.*)))
        *(.text .text.*)
    } >SSM_CODE

    .plt  : { *(.plt)  } >SSM_CODE
    .init : { *(.init) } >SSM_CODE
    .fini : { *(.fini) } >SSM_CODE

    /* .data in Super System data space.  */
    .data :
    {
        ___data_start = ADDR (.data);
        *(.data)
        *(.data.*)
        ___data_end = .;
    } >SSM_BASIC_RAM

    /* .bss in Super System data space.  */
    .bss : ALIGN (0x10)
    {
        ___bss_start = ADDR (.bss);
        *(.bss)
        *(.bss.*)
        *(COMMON)
        ___bss_end = .;
    } >SSM_BASIC_RAM

    /* .rodata in Super System data space.  */
    .rodata : ALIGN (0x10)
    {
        ___rodata_start = ADDR (.rodata);
        *(.rodata)
        *(.rodata.*)
        ___rodata_end = .;
    } >SSM_BASIC_CONST

    /* .ctor/.dtor in Super System data space. */
    .ctor : { *(.ctor) } >SSM_BASIC_CONST
    .dtor : { *(.dtor) } >SSM_BASIC_CONST

    PROVIDE (___heap_start = ALIGN (., 0x10));

    /* Handle .mrk3.records sections.  These contain meta-information
       that is requried for linker relaxation.  After that this
       information is no longer required, but should be harmless if
       kept around.  */
    .mrk3.records 0 : { *(.mrk3.records) }

    /* DWARF debug sections.
       Symbols in the DWARF debugging sections are relative to the beginning
       of the section so we begin them at 0.  */
    /* DWARF 1 */
    .debug          0 : { *(.debug) }
    .line           0 : { *(.line) }
    /* GNU DWARF 1 extensions */
    .debug_srcinfo  0 : { *(.debug_srcinfo) }
    .debug_sfnames  0 : { *(.debug_sfnames) }
    /* DWARF 1.1 and DWARF 2 */
    .debug_aranges  0 : { *(.debug_aranges) }
    .debug_pubnames 0 : { *(.debug_pubnames) }
    /* DWARF 2 */
    .debug_info     0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev   0 : { *(.debug_abbrev) }
    .debug_line     0 : { *(.debug_line) }
    .debug_frame    0 : { *(.debug_frame) }
    .debug_str      0 : { *(.debug_str) }
    .debug_loc      0 : { *(.debug_loc) }
    .debug_macinfo  0 : { *(.debug_macinfo) }
    /* DWARF 3 */
    .debug_pubtypes 0 : { *(.debug_pubtypes) }
    .debug_ranges   0 : { *(.debug_ranges) }
    /* DWARF Extension.  */
    .debug_macro    0 : { *(.debug_macro) }
    .gnu.attributes 0 : { KEEP (*(.gnu.attributes)) }
    /DISCARD/         : { *(.note.GNU-stack)
                          *(.gnu_debuglink)
                          *(.gnu.lto_*)
                          *(.gnu_object_only)
                          *(.comment) }
}

/* 4K stack down to 0x1000 */
PROVIDE (___STACK_END = (0x1ffe));
PROVIDE (___heap_end = (0x1000));
EOF
