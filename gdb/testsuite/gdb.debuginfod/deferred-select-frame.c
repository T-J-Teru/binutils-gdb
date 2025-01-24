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

/* ... */

typedef void (*callback_t) (void);
extern void library1_function (callback_t cb);
extern void library2_function (callback_t cb);

volatile int global_var;

void
f0 (void)
{
  global_var = 42;	/* Breakpoint in f0.  */
}

void
f1 (void)
{
  f0 ();	/* Breakpoint in f1.  */
}

void
f3 (void)
{
  library2_function (f1);
}

void
f4 (void)
{
  f3 ();
}

void
f5 (void)
{
  library1_function (f4);
}

int
main (void)
{
  f5 ();
  return global_var - 42;
}
