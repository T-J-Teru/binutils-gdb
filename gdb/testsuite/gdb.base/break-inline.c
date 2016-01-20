/* This testcase is part of GDB, the GNU debugger.

   Copyright (C) 2012-2016 Free Software Foundation, Inc.

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

static volatile int g;
static volatile int h;

static inline void
foo (void)
{
  g = 42;
}

static inline void
bar (void)
{
  volatile int i;
  for (i = 0; i < 10; ++i);
}

static inline void
baz (void)
{
  g = 24;
  h = 5;
}

__attribute__((noinline)) int
test_inline (void)
{
  foo (); /* location 1 */
  bar (); /* location 2 */
  baz (); /* location 3 */
  return g;
}


int
main (int argc, char *argv[])
{
  int val = test_inline ();
  return val;
}

