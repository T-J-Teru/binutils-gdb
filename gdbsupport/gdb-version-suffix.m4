dnl Autoconf configure script for GDB, the GNU debugger.
dnl Copyright (C) 2022-2023 Free Software Foundation, Inc.
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

dnl Add support for the --with-gdb-version-suffix configure option.  Allows
dnl the user to specify a string that is appended to the version number.
dnl
dnl The intention of this configure option is to allow downstream
dnl distributions of GDB to extend the version string with a distro
dnl specific version number, or patch level.
dnl
dnl However, care should be taken when selecting a suitable string to
dnl append to the version number.  It is best to avoid using strings
dnl that might be confused with an upstream .z release.
dnl
dnl As an example, using --with-gdb-version-suffix=".5", would probably
dnl be a bad idea, this would result in a GDB version number that looks
dnl something like '13.1.5', which could be confused with an upstream
dnl x.y.z release, and if upstream GDB ever released a '13.1.5' then
dnl this could get very confusing.
dnl
dnl Better would be to use something like "-p5", which would give
dnl "13.1-p5", as this is very unlikely to ever match an official
dnl upstream GDB release.
AC_DEFUN([GDB_VERSION_SUFFIX],[
  AC_ARG_WITH(gdb-version-suffix,
    AS_HELP_STRING([--with-gdb-version-suffix=STRING],
                   [Append STRING to the end of the version number]),
  [case "$withval" in
    yes) AC_MSG_ERROR([version suffix not specified]) ;;
    no)  gdb_version_suffix= ;;
    *)   gdb_version_suffix="$withval" ;;
   esac],
    gdb_version_suffix="")
  AC_SUBST(gdb_version_suffix)
])
