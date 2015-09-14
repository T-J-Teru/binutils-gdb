# Script for generating linker script for MRK3
# (This linker script is not parameratised at all)

cat <<EOF
OUTPUT_FORMAT("elf64-mrk3", "elf64-mrk3", "elf64-mrk3")
OUTPUT_ARCH(mrk3)

/* Set up some symbols to be used in building the origin addresses of
   memory regions.  The memory space id values used here must not
   change, they are known throughout the toolchain.*/

MRK3_MEM_SPACE_SYS  = 0x1000000000000000;
MRK3_MEM_SPACE_USER = 0x2000000000000000;
MRK3_MEM_SPACE_SSYS = 0x4000000000000000;

MRK3_MEM_TYPE_DATA  = 0x0000000000000000;
MRK3_MEM_TYPE_CODE  = 0x0100000000000000;

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
        KEEP (*(SORT(.text.libc.string.memcpy*)))
        KEEP (*(SORT(.text.libc.string.memset*)))
        KEEP (*(SORT(.text.libc.strtol.*)))
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
  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.gnu_object_only) *(.comment)}
}

PROVIDE (___STACK_END = (0xfff8));
PROVIDE (___heap_end = (___STACK_END + ___heap_start) / 2);
EOF
