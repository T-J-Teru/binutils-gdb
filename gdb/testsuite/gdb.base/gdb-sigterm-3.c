/* This testcase is part of GDB, the GNU debugger.

   Copyright 2013-2023 Free Software Foundation, Inc.

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

#include <unistd.h>

#if defined(__s390__) || defined(__s390x__)
#define NOP asm("nopr 0")
#elif defined(__or1k__)
#define NOP asm("l.nop")
#else
#define NOP asm("nop")
#endif

#define NOP10 NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP; NOP

int
main (void)
{
  /* Allow for as much timeout as DejaGnu wants, plus a bit of
     slack.  */
  unsigned int seconds = TIMEOUT + 20;

  alarm (seconds);

  for (;;) /* loop-line */
    {
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
      NOP10;
    }
}
