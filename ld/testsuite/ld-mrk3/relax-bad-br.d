#name: Linker relaxation, out of range branch.
#ld: -Trelax-bad-br.ld -relax -q
#objdump: -r

.*:     file format elf64-mrk3

RELOCATION RECORDS FOR \[\.text\]:
OFFSET           TYPE              VALUE 
0000000000000000 R_MRK3_PCREL8     target1
0000000000000002 R_MRK3_PCREL16    target2
0000000000000006 R_MRK3_PCREL8     target3
0000000000000008 R_MRK3_PCREL8     target3


