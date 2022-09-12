# Copyright 2019-2022 Free Software Foundation, Inc.
#
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

# Check that 'info types' produces the expected output for an inferior
# containing a number of different types.


# Run the 'info types' command, and place the results into RESULT_VAR,
# which should be the name of a variable in the callers scope.
#
# RESULT_VAR will be cleared, and set to an associative array, the
# keys of this associative array will be the line numbers, and the
# value for each line number will be a list of the types GDB
# understands to be declared on that line.
#
# Some types don't have a line number for their declartion
# (e.g. builtin types), these are placed into a list for the special
# line number 'NONE'.
#
# Finally, only types from the reported in SRCFILE are collected, any
# types reported from other files (e.g. libraries linked into the
# test) should be ignored.
proc collect_output { result_var } {
    upvar $result_var result_obj

    array set result_obj {}

    set collect_lines false
    gdb_test_multiple "info types" "" {
	-re "^info types\r\n" {
	    exp_continue
	}

	-re "^\r\n" {
	    exp_continue
	}

	-re "^All defined types:\r\n" {
	    exp_continue
	}

	-re "^File (\[^\r\n\]+)\r\n" {
	    set filename $expect_out(1,string)
	    set collect_lines [regexp [string_to_regexp $::srcfile] $filename]
	    exp_continue
	}

	-re "^($::decimal):\\s+(\[^\r\n\]+);\r\n" {
	    set lineno $expect_out(1,string)
	    set text $expect_out(2,string)
	    if { $collect_lines } {
		if { ! [info exists result_obj($lineno)] } {
		    set result_obj($lineno) [list]
		}
		lappend result_obj($lineno) $text
	    }
	    exp_continue
	}

	-re "^\\s+(\[^\r\n;\]+)\r\n" {
	    set text $expect_out(1,string)
	    if { $collect_lines } {
		if { ![info exists result_obj(NONE)] } {
		    set result_obj(NONE) [list]
		}
		lappend result_obj(NONE) $text
	    }
	    exp_continue
	}

	-re "^$::gdb_prompt $" {
	}
    }
}

# RESULT_VAR is the name of a variable in the parent scope that
# contains an associative array of results as returned from the
# collect_output proc.
#
# LINE is a line number, or the empty string, and TEXT is the
# declaration of a type that GDB should have seen on that line.
#
# This proc checks in RESULT_VAR for a matching entry to LINE and
# TEXT, and, if a matching entry is found, calls pass, otherwise, fail
# is called.  The testname used for the pass/fail call is based on
# LINE and TEXT.
#
# If LINE is the empty string then this proc looks for a result
# associated with the special line number 'NONE', see collect_output
# for more details.
proc require_line { result_var line text } {
    upvar $result_var result_obj

    set testname "check for $text"
    if { $line != "" } {
	set testname "$testname on line $line"
    } else {
	set line "NONE"
    }

    if { ![info exists result_obj($line)] } {
	fail $testname
	return
    }

    if { [lsearch -exact $result_obj($line) $text] < 0 } {
	fail $testname
	return
    }

    pass $testname
}

# Run 'info types' test, compiling the test file for language LANG,
# which should be either 'c' or 'c++'.
proc run_test { lang } {
    global testfile
    global srcfile
    global binfile
    global subdir
    global srcdir
    global compile_flags

    standard_testfile info-types.c

    if {[prepare_for_testing "failed to prepare" \
	     "${testfile}" $srcfile "debug $lang"]} {
	return -1
    }
    gdb_test_no_output "set auto-solib-add off"

    if ![runto_main] then {
	return 0
    }

    # Run 'info types' and place the results in RESULT_OBJ.
    collect_output result_obj

    # Some results are common to C and C++.  These are the builtin
    # types which are not declared on any specific line.
    require_line result_obj "" "double"
    require_line result_obj "" "float"
    require_line result_obj "" "int"

    # These typedefs are common to C and C++, but GDB should see a
    # specific line number for these type declarations.
    require_line result_obj "16" "typedef int my_int_t"
    require_line result_obj "17" "typedef float my_float_t"
    require_line result_obj "18" "typedef int nested_int_t"
    require_line result_obj "19" "typedef float nested_float_t"

    if { $lang == "c++" } {
	# These results are specific to 'C++'.
	require_line result_obj "21" "baz_t"
	require_line result_obj "27" "typedef baz_t baz_t"
	require_line result_obj "28" "typedef baz_t baz"
	require_line result_obj "29" "typedef baz_t nested_baz_t"
	require_line result_obj "30" "typedef baz_t nested_baz"
	require_line result_obj "31" "typedef baz_t * baz_ptr"
	require_line result_obj "33" "enum_t"
	require_line result_obj "38" "typedef enum_t my_enum_t"
	require_line result_obj "39" "typedef enum_t nested_enum_t"
	require_line result_obj "52" "typedef enum {...} anon_enum_t"
	require_line result_obj "54" "typedef enum {...} nested_anon_enum_t"
	require_line result_obj "56" "union_t"
	require_line result_obj "62" "typedef union_t nested_union_t"
	require_line result_obj "98" "CL"
	require_line result_obj "103" "typedef CL my_cl"
	require_line result_obj "104" "typedef CL * my_ptr"

	if { [test_compiler_info "gcc-*"] } {
	    # GCC give a name to anonymous structs and unions.  Not
	    # all compilers do this, e.g. Clang does not.
	    require_line result_obj "42" "anon_struct_t"
	    require_line result_obj "45" "typedef anon_struct_t anon_struct_t"
	    require_line result_obj "47" "typedef anon_struct_t nested_anon_struct_t"
	    require_line result_obj "65" "anon_union_t"
	    require_line result_obj "68" "typedef anon_union_t anon_union_t"
	    require_line result_obj "70" "typedef anon_union_t nested_anon_union_t"
	}

    } else {
	# These results are specific to 'C'.
	require_line result_obj "21" "struct baz_t"
	require_line result_obj "28" "typedef struct baz_t baz"
	require_line result_obj "29" "typedef struct baz_t nested_baz_t"
	require_line result_obj "30" "typedef struct baz_t nested_baz"
	require_line result_obj "31" "typedef struct baz_t * baz_ptr"
	require_line result_obj "33" "enum enum_t"
	require_line result_obj "38" "typedef enum enum_t my_enum_t"
	require_line result_obj "39" "typedef enum enum_t nested_enum_t"
	require_line result_obj "45" "typedef struct {...} anon_struct_t"
	require_line result_obj "47" "typedef struct {...} nested_anon_struct_t"
	require_line result_obj "52" "typedef enum {...} anon_enum_t"
	require_line result_obj "54" "typedef enum {...} nested_anon_enum_t"
	require_line result_obj "56" "union union_t"
	require_line result_obj "62" "typedef union union_t nested_union_t"
	require_line result_obj "68" "typedef union {...} anon_union_t"
	require_line result_obj "70" "typedef union {...} nested_anon_union_t"
    }
}

run_test $lang
