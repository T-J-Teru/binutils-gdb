/* This testcase is part of GDB, the GNU debugger.

   Copyright 2018 Free Software Foundation, Inc.

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

/* This program is used to test the ability of GDB to pass some special
   kinds of structures to GDB on RiscV.  The ABI calls for special
   handling of some structures, and these are tested here.  */

#include <string.h>

struct struct_f_f
{
  struct
  {
    float f1;
  } s1;

  struct
  {

  } se1;

  struct
  {
    struct
    {
      float f2;
    } s3;
  } s2;
};

struct struct_d_d
{
  struct
  {
    double f1;
  } s1;

  struct
  {

  } se1;

  struct
  {
    struct
    {
      double f2;
    } s3;
  } s2;
};

struct struct_ld_ld
{
  struct
  {
    long double f1;
  } s1;

  struct
  {

  } se1;

  struct
  {
    struct
    {
      long double f2;
    } s3;
  } s2;
};

struct struct_f_f f_f_val1 = { { 5.02 }, { }, { { 3.14 } } };
struct struct_d_d d_d_val1 = { { 6.25 }, { }, { { 2.21 } } };
struct struct_ld_ld ld_ld_val1 = { { 7.60 }, { }, { { 4.98 } } };

#ifdef TEST_COMPLEX
struct struct_fc
{
  struct
  {
  } se1;

  struct
  {
    struct
    {
      float _Complex fc;
    } s2;
  } s1;

  struct
  {
  } se2;
};

struct struct_dc
{
  struct
  {
  } se1;

  struct
  {
    struct
    {
      double _Complex dc;
    } s2;
  } s1;

  struct
  {
  } se2;
};

struct struct_ldc
{
  struct
  {
  } se1;

  struct
  {
    struct
    {
      long double _Complex dc;
    } s2;
  } s1;

  struct
  {
  } se2;
};

struct struct_fc fc_val1 = {{}, {{1.0F + 1.0iF}}, {}};
struct struct_dc dc_val1 = {{}, {{2.3 + 2.6i}}, {}};
struct struct_ldc ldc_val1 = {{}, {{8.4 + 3.4iF}}, {}};

int
handle_single_fc (struct struct_fc arg1)
{
  return memcmp (&arg1, &fc_val1, sizeof (arg1)) == 0;
}

int
handle_single_dc (struct struct_dc arg1)
{
  return memcmp (&arg1, &dc_val1, sizeof (arg1)) == 0;
}

int
handle_single_ldc (struct struct_ldc arg1)
{
  return memcmp (&arg1, &ldc_val1, sizeof (arg1)) == 0;
}
#endif /* TEST_COMPLEX */

int
handle_single_f_f (struct struct_f_f arg1)
{
  return memcmp (&arg1, &f_f_val1, sizeof (arg1)) == 0;
}

int
handle_single_d_d (struct struct_d_d arg1)
{
  return memcmp (&arg1, &d_d_val1, sizeof (arg1)) == 0;
}

int
handle_single_ld_ld (struct struct_ld_ld arg1)
{
  return memcmp (&arg1, &ld_ld_val1, sizeof (arg1)) == 0;
}

void
breakpt (void)
{
  asm ("" ::: "memory");
}

int
main ()
{
  breakpt ();
  return 0;
}
