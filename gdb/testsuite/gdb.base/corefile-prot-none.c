/* Copyright 2025 Free Software Foundation, Inc.

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

#include <sys/mman.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <assert.h>

void
breakpt ()
{
  /* Nothing.  */
}

int
main ()
{
  void *addr[2];
  int pg_sz = getpagesize ();

  for (int i = 0; i < 2; ++i)
    {
      /* Create anonymous mapping as read/write.  */
      addr[i] = mmap (NULL, pg_sz, PROT_READ | PROT_WRITE,
		      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
      assert (addr[i] != MAP_FAILED);

      /* For the first mapping only, write to the mapping.  */
      if (i == 0)
	*((int *) addr[i]) = 123;

      /* Set every mapping PROT_NONE.  */
      int res = mprotect (addr[i], pg_sz, PROT_NONE);
      assert (res == 0);
    }

  breakpt ();

  abort ();

  return 0;
}
