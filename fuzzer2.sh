#!/bin/sh

srcdir=$(cd $(dirname $0) && pwd)
includedir=$srcdir/include
topdir=$(dirname $srcdir)
builddir=$topdir/build
sources="$srcdir/fuzzer2.c $builddir/libiberty/libiberty.a"
fuzzer=$builddir/fuzzer2

set -ex
gcc -g -O0 -I$includedir $sources -o $fuzzer -ldl

