#PROG: objcopy
#source: remove-relocs-07.s
#objcopy: --remove-section=.reladata.relocs.01 --remove-section=.reldata.relocs.01
#readelf: -r

Relocation section '\.rela?data\.relocs\.02' at offset 0x[0-9a-f]+ contains 3 entries:
.*
.*
.*
.*
