# AM_GDB_CHECK_FOR_BROKEN_LTO
# ---------------------------

# Checks if the C++ compiler is GCC 14 (any minor version) AND if
# CXXFLAGS is configured for Link-Time Optimization (LTO).  If these
# two conditons are met, then an error is raised, and configuration
# will stop.  Otherwise, configuration can continue.
#
# There is a known bug in gcc 14 (at least .1, .2, and .3) which
# breaks GDB when compiled with LTO.  For details, see:
# https://sourceware.org/bugzilla/show_bug.cgi?id=32571
#
AC_DEFUN([AM_GDB_CHECK_FOR_BROKEN_LTO],[

  # Some requirements.  Do this before AC_MSG_CHECKING so that
  # the configure output looks neater.
  AM_GDB_COMPILER_TYPE
  AC_PROG_CXX
  AC_CHECK_PROG([READELF], [readelf], [readelf])

  AC_MSG_CHECKING([whether LTO in use, and known to be broken])

  is_lto_broken=no

  AS_IF([test "$GDB_COMPILER_TYPE" == "gcc"],
  [ # This is GCC, now check if this is gcc-14 specifically.
    AC_LANG_PUSH(C++)
    is_gcc_14=no
    AC_COMPILE_IFELSE(
       [AC_LANG_PROGRAM([], [
       #if __GNUC__ == 14
       #  error "lto is broken for gcc 14"
       #endif
       ])],
       [is_gcc_14=no],
       [is_gcc_14=yes])
    AC_LANG_POP(C++)

     AS_IF([test "$is_gcc_14" = "yes"],
     [ # Is gcc-14, check if LTO is in use.
       AS_IF([test -z "$READELF"],
       [
         AC_MSG_WARN([readelf not found, cannot detect LTO.])
	 is_lto_broken=no
       ],
       [ # We have readelf so we can check sections in the generated object.
	 AC_LANG_PUSH(C++)

         # Compile a dummy C++ program to an object file using current CXXFLAGS.
	 AC_COMPILE_IFELSE([AC_LANG_PROGRAM([], [])],
         [ # Success: compilation worked. Now check the object file for LTO sections.
           AS_IF(["$READELF" -S conftest.$OBJEXT | grep -q "\.gnu\.lto_\.opts"],
           [ # LTO section found (and we know it's GCC 14)
	     is_lto_broken=yes
           ],
           [ # LTO section not found (but it's GCC 14)
	     is_lto_broken=no
           ])
         ],
	 [ # The empty program failed to compile. Just assume LTO (if
	   # in use) is fine.
	   AC_MSG_WARN([failed to compile test program, cannot detect LTO.])
	   is_lto_broken=no
	 ])

	 AC_LANG_POP(C++)
       ])
     ],
     [ # For other gcc versions, LTO is assumed good.
       is_lto_broken=no
     ])
  ],
  [ # Compiler is not gcc, assume lto is fine.
    is_lto_broken=no
  ])

  AC_MSG_RESULT([$is_lto_broken])
  AS_IF([test "$is_lto_broken" = "yes"],
  [
    AC_MSG_ERROR([
LTO on GCC 14 contains known bugs which prevent GDB from working correctly,
see https://sourceware.org/bugzilla/show_bug.cgi?id=32571 for details.  Either
use a different version of GCC, or disable LTO.], [1])
  ],[])
])
