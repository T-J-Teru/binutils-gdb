/* This file is part of GDB, the GNU debugger.

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

#include <riscv_vector.h>
#include <malloc.h>

unsigned
do_vlen_read ()
{
  unsigned vlenb;
  asm volatile ("csrr %[vlenb], vlenb" : [vlenb] "=r"(vlenb) : :);
  /* According to vector spec: "vlenb holds the value VLEN/8".  */
  return vlenb * 8;
}

vint64m1_t
foo (vint64m1_t a, vint32mf2_t b, vint64m1_t c, size_t n)
{
  vint64m1_t tmp = __riscv_vwadd_wv_i64m1 (a, b, n);
  return __riscv_vadd_vv_i64m1 (c, tmp, n);
}

vint32m4_t
foo1 (vint32m4_t a, vint16m2_t b, size_t n)
{
  return __riscv_vwadd_wv_i32m4 (a, b, n);
}

vint32m4_t
foo2 (vint16m2_t a, vint32m4_t b, size_t n)
{
  return __riscv_vwadd_wv_i32m4 (b, a, n);
}

vint64m8_t
foo3 (vint64m8_t a, vint64m8_t b, vint64m8_t c, size_t n)
{
  vint64m8_t tmp = __riscv_vadd_vv_i64m8 (a, b, n);
  return __riscv_vadd_vv_i64m8 (tmp, c, n);
}

vint64m8_t
foo4 (vint64m8_t a, vint64m8_t b, vbool8_t mask, vbool8_t mask2, size_t n)
{
  return __riscv_vadd_vv_i64m8_m (mask2, a, b, n);
}

vint32m4_t
foo5_get0 (vint16m2_t tmp_a, vint32m4x2_t a)
{
  return __riscv_vget_v_i32m4x2_i32m4 (a, 0);
}

vint32m4_t
foo5_get1 (vint16m2_t tmp_a, vint32m4x2_t a)
{
  return __riscv_vget_v_i32m4x2_i32m4 (a, 1);
}

vint32mf2_t
foo6_get0 (vint16m2_t tmp_a, vint32mf2x2_t a)
{
  return __riscv_vget_v_i32mf2x2_i32mf2 (a, 0);
}

vint32mf2_t
foo6_get1 (vint16m2_t tmp_a, vint32mf2x2_t a)
{
  return __riscv_vget_v_i32mf2x2_i32mf2 (a, 1);
}

int
main ()
{
  unsigned n = do_vlen_read () / 64;
  vint64m1_t a = __riscv_vmv_v_x_i64m1 (42, n);
  vint32mf2_t b = __riscv_vmv_v_x_i32mf2 (43, n);
  vint64m1_t c = __riscv_vmv_v_x_i64m1 (44, n);

  vint64m1_t res = foo (a, b, c, n);
  /* break 2 */

  n = do_vlen_read () * 4 / 32;
  vint32m4_t g = __riscv_vmv_v_x_i32m4 (48, n);
  vint16m2_t h = __riscv_vmv_v_x_i16m2 (49, n);

  vint32m4_t res_1 = foo1 (g, h, n); // g is on v8-v11, h is on v12-v13
  vint32m4_t res_2 = foo2 (h, g, n); // h is on v8-v9,  g is on v12-v15
  /* break 3 */

  n = do_vlen_read () * 8 / 64;
  vint64m8_t big1 = __riscv_vmv_v_x_i64m8 (50, n);
  vint64m8_t big2 = __riscv_vmv_v_x_i64m8 (51, n);
  vint64m8_t big3 = __riscv_vmv_v_x_i64m8 (52, n);
  vint64m8_t big_res = foo3 (big1, big2, big3, n);
  /* break 4 */

  unsigned mask_size = n / 8;
  uint8_t *rs1_mask = malloc (mask_size * sizeof (uint8_t));
  for (int i = 0; i < mask_size; i++)
    rs1_mask[i] = 0xa5;

  vbool8_t mask = __riscv_vlm_v_b8 (rs1_mask, n);
  vint64m8_t masked_sum = foo4 (big1, big2, mask, mask, n);
  /* break 5 */

  n = do_vlen_read () * 4 * 2 / 32;
  unsigned addr_size = n / 2;
  uint32_t *addr = malloc (addr_size * sizeof (uint32_t));
  for (unsigned i = 0; i < addr_size; i++)
    addr[i] = 8 * (uint32_t) i;

  vuint32m4_t rs2 = __riscv_vle32_v_u32m4 (addr, addr_size);
  /* break 6 */

  int32_t *rs1 = malloc (n * sizeof (*rs1));
  for (int i = 0; i < n; i++)
    rs1[i] = (int32_t) i;

  vint32m4x2_t a_seg = __riscv_vluxseg2ei32_v_i32m4x2 (rs1, rs2, n);
  /* break 7 */
  vint32m4_t res_a_seg0 = foo5_get0 (h, a_seg);
  /* break 8 */
  vint32m4_t res_a_seg1 = foo5_get1 (h, a_seg);
  /* break 9 */

  vuint32mf2_t rs2_2 = __riscv_vle32_v_u32mf2 (addr, addr_size);
  /* break 10 */

  n = do_vlen_read () / 32;
  vint32mf2x2_t b_seg = __riscv_vluxseg2ei32_v_i32mf2x2 (rs1, rs2_2, n);
  /* break 11 */
  vint32mf2_t res_b_seg0 = foo6_get0 (h, b_seg);
  /* break 12 */
  vint32mf2_t res_b_seg1 = foo6_get1 (h, b_seg);
  /* break 13 */

  free (rs1_mask);
  free (addr);
  free (rs1);

  return 0;
}
