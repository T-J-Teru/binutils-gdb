#source: group1a.s
#source: group1b.s
#ld: -T group.ld --inhibit-group-allocation
#readelf: -g
#xfail: d30v-*-* dlx-*-* i960-*-* pj*-*-*
# generic linker targets don't comply with all symbol merging rules

COMDAT group section \[ +[0-9]+\] `\.group' \[foo_group\] contains 1 sections:
   \[Index\]    Name
   \[ +[0-9]+\]   \.text
