
.*:     file format elf64-x86-64

Contents of the \.debug_info section:

  Compilation Unit @ offset 0x0:
   Length:        0x49 \(32-bit\)
   Version:       2
   Abbrev Offset: 0x0
   Pointer Size:  4
 <0><b>: Abbrev Number: 1 \(DW_TAG_compile_unit\)
    <c>   DW_AT_ranges      : 0x0
    <10>   DW_AT_name        : file1\.c
    <18>   DW_AT_language    : 1	\(ANSI C\)
 <1><19>: Abbrev Number: 2 \(DW_TAG_subprogram\)
    <1a>   DW_AT_name        : func_cu1
    <23>   DW_AT_type        : <0x45>
    <27>   DW_AT_low_pc      : 0x4
    <2b>   DW_AT_high_pc     : 0x8
 <1><2f>: Abbrev Number: 2 \(DW_TAG_subprogram\)
    <30>   DW_AT_name        : func_cu2
    <39>   DW_AT_type        : <0x45>
    <3d>   DW_AT_low_pc      : 0x8
    <41>   DW_AT_high_pc     : 0xc
 <1><45>: Abbrev Number: 3 \(DW_TAG_base_type\)
    <46>   DW_AT_name        : int
    <4a>   DW_AT_byte_size   : 4
    <4b>   DW_AT_encoding    : 5	\(signed\)
 <1><4c>: Abbrev Number: 0

Contents of the \.debug_ranges section:

    Offset   Begin    End
    00000000 ffffffff 00000004 \(base address\)
    00000000 00000004 00000008 
    00000000 ffffffff 00000008 \(base address\)
    00000000 00000008 0000000c 
    00000000 <End of list>

Contents of the \.debug_abbrev section:

  Number TAG \(0x0\)
   1      DW_TAG_compile_unit    \[has children\]
    DW_AT_ranges       DW_FORM_sec_offset
    DW_AT_name         DW_FORM_string
    DW_AT_language     DW_FORM_data1
    DW_AT value: 0     DW_FORM value: 0
   2      DW_TAG_subprogram    \[no children\]
    DW_AT_name         DW_FORM_string
    DW_AT_type         DW_FORM_ref4
    DW_AT_low_pc       DW_FORM_addr
    DW_AT_high_pc      DW_FORM_addr
    DW_AT value: 0     DW_FORM value: 0
   3      DW_TAG_base_type    \[no children\]
    DW_AT_name         DW_FORM_string
    DW_AT_byte_size    DW_FORM_data1
    DW_AT_encoding     DW_FORM_data1
    DW_AT value: 0     DW_FORM value: 0

Raw dump of debug contents of section \.debug_line:

  Offset:                      0x0
  Length:                      25
  DWARF Version:               2
  Prologue Length:             19
  Minimum Instruction Length:  1
  Initial value of 'is_stmt':  1
  Line Base:                   -5
  Line Range:                  14
  Opcode Base:                 13

 Opcodes:
  Opcode 1 has 0 args
  Opcode 2 has 1 args
  Opcode 3 has 1 args
  Opcode 4 has 1 args
  Opcode 5 has 1 args
  Opcode 6 has 0 args
  Opcode 7 has 0 args
  Opcode 8 has 0 args
  Opcode 9 has 1 args
  Opcode 10 has 0 args
  Opcode 11 has 0 args
  Opcode 12 has 1 args

 The Directory Table is empty\.

 The File Name Table is empty\.

 No Line Number Statements\.
