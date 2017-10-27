#! /bin/sh

# Add a .gdb_index section to a file.

# Copyright (C) 2010-2024 Free Software Foundation, Inc.
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# This program assumes objcopy and readelf are in $PATH.  If not, or
# you want others, pass the following in the environment
OBJCOPY=${OBJCOPY:=objcopy}
READELF=${READELF:=readelf}

myname="${0##*/}"

# For GDB itself we need to be a little smarter.  If GDB is set in the
# environment then we will use that.  But if GDB is not set in the
# environment then we have a couple of options that we need to check
# through.
#
# Our default choice is for /usr/bin/gdb.minimal.  For an explanation
# of why this is chosen, check out:
#   https://bugzilla.redhat.com/show_bug.cgi?id=1695015
#   https://fedoraproject.org/wiki/Changes/Minimal_GDB_in_buildroot
#
# If gdb.minimal is not found then we look for a 'gdb' executable on
# the path.
#
# And finally, we check for /usr/libexec/gdb.
#
# If none of those result in a useable GDB then we give an error and
# exit.
if test -z "$GDB"; then
    for possible_gdb in /usr/bin/gdb.minimal gdb /usr/libexec/gdb; do
        if ! command -v "$possible_gdb" >/dev/null 2>&1; then
            continue
        fi

        possible_gdb=$(command -v "$possible_gdb")

        if ! test -x "$possible_gdb"; then
            continue
        fi

        GDB="$possible_gdb"
        break
    done

    if test -z "$GDB"; then
        echo "$myname: Failed to find a useable GDB binary" 1>&2
        exit 1
    fi
fi

dwarf5=""
if [ "$1" = "-dwarf-5" ]; then
    dwarf5="$1"
    shift
fi

if test $# != 1; then
    echo "usage: $myname [-dwarf-5] FILE" 1>&2
    exit 1
fi

file="$1"

if test -L "$file"; then
    if ! command -v readlink >/dev/null 2>&1; then
	echo "$myname: 'readlink' missing.  Failed to follow symlink $1." 1>&2
	exit 1
    fi

    # Count number of links followed in order to detect loops.
    count=0
    while test -L "$file"; do
	target=$(readlink "$file")

	case "$target" in
	    /*)
		file="$target"
		;;
	    *)
		file="$(dirname "$file")/$target"
		;;
	esac

	count="$((count + 1))"
	if test "$count" -gt 10; then
	    echo "$myname: Detected loop while following link $file"
	    exit 1
	fi
    done
fi

if test ! -r "$file"; then
    echo "$myname: unable to access: $file" 1>&2
    exit 1
fi

dir="${file%/*}"
test "$dir" = "$file" && dir="."

dwz_file=""
if $READELF -S "$file" | grep -q " \.gnu_debugaltlink "; then
    dwz_file=$($READELF --string-dump=.gnu_debugaltlink "$file" \
		   | grep -A1  "'\.gnu_debugaltlink':" \
		   | tail -n +2 \
		   | sed 's/.*]//')
    dwz_file=$(echo $dwz_file)
    if $READELF -S "$dwz_file" | grep -E -q " \.(gdb_index|debug_names) "; then
	# Already has an index, skip it.
	dwz_file=""
    fi
fi

set_files ()
{
    fpath="$1"

    index4="${fpath}.gdb-index"
    index5="${fpath}.debug_names"
    debugstr="${fpath}.debug_str"
    debugstrmerge="${fpath}.debug_str.merge"
    debugstrerr="${fpath}.debug_str.err"
}

tmp_files=
for f in "$file" "$dwz_file"; do
    if [ "$f" = "" ]; then
	continue
    fi
    set_files "$f"
    tmp_files="$tmp_files $index4 $index5 $debugstr $debugstrmerge $debugstrerr"
done

rm -f $tmp_files

# Ensure intermediate index file is removed when we exit.
trap "rm -f $tmp_files" 0

$GDB --batch -nx -iex 'set auto-load no' \
    -iex 'set debuginfod enabled off' \
    -ex "file $file" -ex "save gdb-index $dwarf5 $dir" || {
    # Just in case.
    status=$?
    echo "$myname: gdb error generating index for $file" 1>&2
    exit $status
}

# In some situations gdb can exit without creating an index.  This is
# not an error.
# E.g., if $file is stripped.  This behaviour is akin to stripping an
# already stripped binary, it's a no-op.
status=0

handle_file ()
{
    fpath="$1"

    set_files "$fpath"

    if test -f "$index4" -a -f "$index5"; then
	echo "$myname: Both index types were created for $fpath" 1>&2
	status=1
    elif test -f "$index4" -o -f "$index5"; then
	if test -f "$index4"; then
	    index="$index4"
	    section=".gdb_index"
	else
	    index="$index5"
	    section=".debug_names"
	fi
	debugstradd=false
	debugstrupdate=false
	if test -s "$debugstr"; then
	    if ! $OBJCOPY --dump-section .debug_str="$debugstrmerge" "$fpath" \
		 /dev/null 2>$debugstrerr; then
		cat >&2 $debugstrerr
		exit 1
	    fi
	    if grep -q "can't dump section '.debug_str' - it does not exist" \
		    $debugstrerr; then
		debugstradd=true
	    else
		debugstrupdate=true
		cat >&2 $debugstrerr
	    fi
	    cat "$debugstr" >>"$debugstrmerge"
	fi

	$OBJCOPY --add-section $section="$index" \
		 --set-section-flags $section=readonly \
		 $(if $debugstradd; then \
		       echo --add-section .debug_str="$debugstrmerge"; \
		       echo --set-section-flags .debug_str=readonly; \
		   fi; \
		   if $debugstrupdate; then \
		       echo --update-section .debug_str="$debugstrmerge"; \
		   fi) \
		 "$fpath" "$fpath"

	status=$?
    else
	echo "$myname: No index was created for $fpath" 1>&2
	echo "$myname: [Was there no debuginfo? Was there already an index?]" \
	     1>&2
    fi
}

handle_file "$file"
if [ "$dwz_file" != "" ]; then
    handle_file "$dwz_file"
fi

exit $status
