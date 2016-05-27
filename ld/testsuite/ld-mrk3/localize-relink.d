#name: Relinking localized data
#ld: -q
#objcopy: -G foo
#ld:
#objdump: -D -j .debug_hello

.*:     file format elf64-mrk3

Disassembly of section .debug_hello:

0000000000000000 <.debug_hello>:
   0:   02 01 00 00     .word   0x00000102
   4:   01 00 00 10     .word   0x10000001

