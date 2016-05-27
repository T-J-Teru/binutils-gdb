	.type	a,@object               ; @a
	.data
	.globl	a
	.align	1
a:
	.short	2                       ; 0x2
	.size	a, 2

	.type	b,@object               ; @b
	.globl	b
	.align	1
b:
	.short	3                       ; 0x3
	.size	b, 2

	.type	c,@object               ; @c
	.globl	c
	.align	1
c:
	.short	4                       ; 0x4
	.size	c, 2

	.section	.debug_hello
  .quad b
