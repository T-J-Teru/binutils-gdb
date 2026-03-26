/* This testcase is part of GDB, the GNU debugger.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

struct S {
  int x;
  explicit S (int n = 0) : x (n) {}
  S operator+ (int n) const { return S (x + n); }
};

typedef S S_td;
using S_u = S;

static int add (const struct S &s1, const struct S &s2) {
  return s1.x + s2.x;
}

/* Non-trivially copyable: copy-constructing swaps the two members.  */
struct swapcopy {
  int lo;
  int hi;
  swapcopy (int l, int h) : lo (l), hi (h) {}
  swapcopy (const swapcopy &o) : lo (o.hi), hi (o.lo) {}
};

/* Pass swapcopy by value so the call must copy-construct the argument.  */

static int
swapcopy_first_byval (swapcopy c)
{
  return c.lo;
}

struct Base {
  int x;
  Base () : x (0) {}
  explicit Base (int n) : x (n) {}
  Base (const Base &other) : x (other.x) {}
};

typedef Base Base_td;
using Base_u = Base;

namespace NS {
class Derived : public Base {
public:
  int y;
  Derived () : Base (), y (0) {}
  Derived (int a, int b) : Base (a), y (b) {}
};

typedef Base NsBaseTd;
using NsBaseU = Base;
typedef Derived Derived_td;
using Derived_u = Derived;

union U {
  int a;
  U () : a (0) {}
  explicit U (int n) : a (n) {}
};

typedef U Nu_td;
}

union U {
  int x;
  U () : x (0) {}
  explicit U (int n) : x (n) {}
};

typedef U U_td;

static int plus_one (U u)
{
  return u.x + 1;
}

int
main (void)
{
  S s0;           /* default: x = 0 */
  S s1 (42);     /* x = 42 */
  S s2 (s1 + 2); /* x = 44 */
  S s3 = s1;
  NS::Derived d;	     /* Base part x=0, Derived part y=0  */
  NS::Derived d1 (10, 20);   /* Base part x=10, Derived part y=20  */
  Base b;		     /* x=0  */
  Base b1 (5);               /* x=5  */
  Base b2 (d1);		     /* x=10  */
  U u0;                      /* default: x = 0  */
  U u1 (42);                 /* x = 42  */
  NS::U uv0;                /* default: a = 0  */
  NS::U uv1 (7);            /* a = 7  */
  S_td s_td = S_td (11);
  S_u s_u = S_u (12);
  Base_td b_td = Base_td (8);
  Base_u b_u = Base_u (9);
  NS::NsBaseTd nsb_td = NS::NsBaseTd (13);
  NS::NsBaseU nsb_u = NS::NsBaseU (14);
  NS::Derived_td d_td = NS::Derived_td (2, 3);
  NS::Derived_u d_u = NS::Derived_u (4, 5);
  U_td u_td = U_td (15);
  NS::Nu_td nu_u = NS::Nu_td (16);
  swapcopy swp (30, 40);
  int result = add (s1, s2);
  return result + d.x + d.y + b.x + b1.x + b2.x + d.x + d.y \
	+ d1.x + d1.y + plus_one (u0) + u1.x + uv0.a + uv1.a \
	+ s_td.x + s_u.x + b_td.x + b_u.x + nsb_td.x + nsb_u.x \
	+ d_td.x + d_td.y + d_u.x + d_u.y + u_td.x + nu_u.a \
	+ swapcopy_first_byval (swapcopy (100, 200)) \
	+ swapcopy_first_byval (swp);  /* stop-here */
}
