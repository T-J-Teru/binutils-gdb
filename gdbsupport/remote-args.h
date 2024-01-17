/* Functions to help when passing arguments between GDB and gdbserver.

   Copyright (C) 2023 Free Software Foundation, Inc.

   This file is part of GDB.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

namespace gdb
{

namespace remote_args
{

/* ARGS is an inferior argument string.  This function splits ARGS into
   individual arguments and returns a vector containing each argument.  */

extern std::vector<std::string> split (std::string args);

/* Join together the separate arguments in ARGS and build a single
   inferior argument string.  The string returned by this function will be
   equivalent, but not necessarily identical to the string passed to
   ::split, for example passing the string '"a b"' (without the single
   quotes, but including the double quotes) to ::split, will return an
   argument of 'a b' (without the single quotes).  When this argument is
   passed through ::join we will get back the string 'a\ b' (without the
   single quotes), that is, we choose to escape the white space, rather
   than wrap the argument in quotes.  */
extern std::string join (std::vector<char *> &args);

} /* namespace remote_args */

} /* namespac gdb */
