#!/bin/sh

srcdir=$(cd $(dirname $0) && pwd)
includedir=$srcdir/include
topdir=$(dirname $srcdir)
builddir=$topdir/build
sources="$srcdir/fuzzer.c $builddir/libiberty/libiberty.a"
fuzzer=$builddir/fuzzer

set -ex
gcc -Wall -Werror -g -O0 -I$includedir $sources -o $fuzzer -ldl
/usr/bin/time -p $fuzzer $@
