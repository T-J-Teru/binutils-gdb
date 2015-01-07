#...
Linker script and memory map

                0x0000000000000001                PROVIDE \(foo, 0x1\)
                \[!provide\]                        PROVIDE \(bar, 0x2\)
                0x0000000000000003                PROVIDE \(baz, 0x3\)

\.text           0x0000000000000000        0x0
 \.text          0x0000000000000000        0x0 tmpdir/dump0\.o

\.iplt           0x0000000000000000        0x0
 \.iplt          0x0000000000000000        0x0 tmpdir/dump0\.o

\.rela\.dyn       0x0000000000000000        0x0
 \.rela\.iplt     0x0000000000000000        0x0 tmpdir/dump0\.o
 \.rela\.data     0x0000000000000000        0x0 tmpdir/dump0\.o

\.data           0x0000000000002000       0x10
 \*\(\.data\)
 \.data          0x0000000000002000       0x10 tmpdir/dump0\.o
                0x0000000000002000                foo
                \[!provide\]                        PROVIDE \(loc1, ALIGN \(\., 0x10\)\)
                0x0000000000002010                PROVIDE \(loc2, ALIGN \(\., 0x10\)\)
                \[!provide\]                        PROVIDE \(loc3, \(loc1 \+ 0x20\)\)
                0x0000000000002030                loc4 = \(loc2 \+ 0x20\)
#...