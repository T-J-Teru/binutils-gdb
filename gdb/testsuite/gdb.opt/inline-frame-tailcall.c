/* Copyright 2020 Free Software Foundation, Inc.

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

#ifdef __GNUC__
# define ATTR_INLINE __attribute__((gnu_inline)) __attribute__((always_inline)) __attribute__((noclone))
#else
# define ATTR_INLINE
#endif

volatile int global;

volatile int counter;

static inline ATTR_INLINE int
bar ()
{
  /* Just some filler.  */
  for (counter = 0; counter < 10; ++counter)
    global = 0;
  return 0;
}

__attribute__ ((noinline)) int
foo ()
{
  return bar ();
}

__attribute__ ((noinline)) int
test_func ()
{
  return foo ();
}

int
main ()
{
  global = test_func ();
  return (global * 2);
}
