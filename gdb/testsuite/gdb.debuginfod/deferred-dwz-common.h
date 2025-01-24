/* This testcase is part of GDB, the GNU debugger.

   Copyright 2026 Free Software Foundation, Inc.

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

/* Common type shared between the two libraries.  Having a shared type
   that appears in both libraries gives the DWZ tool something to
   extract into the common DWZ file.  */

struct common_type
{
  int x;
  int y;
};

/* A common function used in both libraries.  Being defined in a
   header file means its debug info will appear in both libraries'
   DWARF, giving DWZ something to deduplicate.  We must force this
   function to be inlined though, as it is the abstact instance of
   this function that will be deduplicate, not the concrete instance.
   If we don't force the function inline, then all we get is a
   non-inline instance within each library, and the DWARF for these
   two instances will not be moved into the DWZ file.  */

static inline int __attribute__((always_inline))
common_add (struct common_type *ct)
{
  return ct->x + ct->y;
}
