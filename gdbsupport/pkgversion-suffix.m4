dnl Autoconf configure script for GDB, the GNU debugger.
dnl Copyright (C) 2022 Free Software Foundation, Inc.
dnl
dnl This file is part of GDB.
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 3 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program.  If not, see <http://www.gnu.org/licenses/>.

dnl Add support for the --with-pkgversion-suffix configure option.  Allows
dnl the user to specify a string that is appended to the version number.
AC_DEFUN([GDB_PKGVERSION_SUFFIX],[
  AC_ARG_WITH(pkgversion-suffix,
    AS_HELP_STRING([--with-pkgversion-suffix=STRING],
                   [Append STRING to the end of the version number]),
  [case "$withval" in
    yes) AC_MSG_ERROR([version suffix not specified]) ;;
    no)  PKGVERSION_SUFFIX= ;;
    *)   PKGVERSION_SUFFIX="$withval" ;;
   esac],
    PKGVERSION_SUFFIX="")
  AC_DEFINE_UNQUOTED([PKGVERSION_SUFFIX], ["$PKGVERSION_SUFFIX"],
                     [A string to append to the version number])
])
