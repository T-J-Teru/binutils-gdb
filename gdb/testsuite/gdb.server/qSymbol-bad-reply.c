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

#include <stdlib.h>
#include <assert.h>
#include <limits.h>

unsigned char *buffer = NULL;
size_t buffer_length = 1024 * 1024 * 100;
volatile int global_var = 0;

/* Is C a byte that must be escaped in a reply from gdbserver to
   GDB?  Return 1 if it is, otherwise, return 0.  */
static int
is_escapable_byte (unsigned char c)
{
  switch (c)
    {
    case 0x23:
    case 0x24:
    case 0x7d:
    case 0x2a:
      return 1;
    default:
      return 0;
    }
}

int
main ()
{
  buffer = malloc ( buffer_length );
  assert (buffer != NULL);

  unsigned char val = 0;
  for (size_t i = 0; i < buffer_length; ++i)
    {
      buffer[i] = val;
      do
	{
	  val = (val + 1) % UCHAR_MAX;
	}
      while (is_escapable_byte (val));
    }

  global_var = 42;	/* Break here.  */

  return 0;
}
