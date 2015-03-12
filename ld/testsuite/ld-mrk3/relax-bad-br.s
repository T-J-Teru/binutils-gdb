        .section ".text", "ax"
        .global _start
_start:
        bra     target1
        bra     target2
        bra     target3
        bra     target3

        .section ".text.1", "ax"
        .global target1
target1:
        nop

        .section ".text.2", "ax"
        .global target2
target2:
        nop

        .section ".text.3", "ax"
        .global target3
target3:
        nop
