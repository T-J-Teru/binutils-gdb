/* BFD library support routines for the MRK3 architecture.
   Copyright 1999, 2000, 2002, 2005, 2006, 2007, 2008
   Free Software Foundation, Inc.
   Contributed by Denis Chertykov <denisc@overta.ru>

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */

#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"

/* Fill buffer with NOPs */
static void *
bfd_arch_mrk3_fill (bfd_size_type count,
		    bfd_boolean is_bigendian ATTRIBUTE_UNUSED,
		    bfd_boolean code ATTRIBUTE_UNUSED)
{
  /* mov.b r0l, r0l */
  static const char nop[] = { 0x40, 0x68 };
  bfd_size_type nop_size = 2;

  void *fill = bfd_malloc (count);
  if (fill != NULL)
    {
      bfd_byte *p = fill;
      while (count >= nop_size) {
	memcpy (p, nop, nop_size);
	p += nop_size;
	count -= nop_size;
      }
      if (count != 0)
	memset (p, 0, count);
    }
  return fill;
}


const bfd_arch_info_type bfd_mrk3_arch =
{
  16,                     /* bits per word */
  32,                     /* bits per address */
  8,                      /* bits per byte */
  bfd_arch_mrk3,          /* architecture */
  bfd_mach_mrk3,          /* machine */
  "mrk3",                 /* architecture name */
  "mrk3",                 /* printable name */
  2,                      /* section align power */
  TRUE,                   /* the default ? */
  bfd_default_compatible, /* architecture comparison fn */
  bfd_default_scan,       /* string to architecture convert fn */
  bfd_arch_mrk3_fill,     /* MRK3 fill.  */
  NULL			  /* next in list */
};
