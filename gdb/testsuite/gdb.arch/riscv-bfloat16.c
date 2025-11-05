/* Test program for bfloat16 support in RISC-V FPU registers.
   Copyright 2025 Free Software Foundation, Inc.

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

/* BFloat16 variable test data */
__bf16 bf16_scalar = 1.5bf16;
__bf16 bf16_array[8] = {
  0.0bf16, 0.5bf16, 1.0bf16, 1.5bf16,
  2.0bf16, 2.5bf16, 3.0bf16, 3.5bf16
};

/* Float test data */
float float_data[] = {
  0.0f, 0.125f, 0.25f, 0.375f,
  0.5f, 0.625f, 0.75f, 0.875f,
  1.0f, 1.125f, 1.25f, 1.375f,
  1.5f, 1.625f, 1.75f, 1.875f,
  2.0f, 2.125f, 2.25f, 2.375f,
  2.5f, 2.625f, 2.75f, 2.875f,
  3.0f, 3.125f, 3.25f, 3.375f,
  3.5f, 3.625f, 3.75f, 3.875f,
};

void
load_float_to_fpu_regs (void)
{
  /* Load float values into FPU registers */
  __asm__ __volatile__ (
    "flw ft0, 0(%0)\n\t"
    "flw ft1, 4(%0)\n\t"
    "flw ft2, 8(%0)\n\t"
    "flw ft3, 12(%0)\n\t"
    "flw ft4, 16(%0)\n\t"
    "flw ft5, 20(%0)\n\t"
    "flw ft6, 24(%0)\n\t"
    "flw ft7, 28(%0)\n\t"
    : /* no outputs */
    : "r" (float_data)
    : "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7"
  );
}

void
convert_float_to_bfloat16 (void)
{
  /* Convert float to bfloat16 using Zfbfmin instructions
     fcvt.bf16.s converts float (in FPU reg) to bfloat16 (in FPU reg) */
  __asm__ __volatile__ (
    "fcvt.bf16.s ft0, ft0\n\t"
    "fcvt.bf16.s ft1, ft1\n\t"
    "fcvt.bf16.s ft2, ft2\n\t"
    "fcvt.bf16.s ft3, ft3\n\t"
    "fcvt.bf16.s ft4, ft4\n\t"
    "fcvt.bf16.s ft5, ft5\n\t"
    "fcvt.bf16.s ft6, ft6\n\t"
    "fcvt.bf16.s ft7, ft7\n\t"
    : /* no outputs */
    : /* no inputs */
    : "ft0", "ft1", "ft2", "ft3", "ft4", "ft5", "ft6", "ft7"
  );
}

void
load_more_floats (void)
{
  /* Load more float values into argument registers */
  __asm__ __volatile__ (
    "flw fa0, 64(%0)\n\t"
    "flw fa1, 68(%0)\n\t"
    "flw fa2, 72(%0)\n\t"
    "flw fa3, 76(%0)\n\t"
    "flw fa4, 80(%0)\n\t"
    "flw fa5, 84(%0)\n\t"
    "flw fa6, 88(%0)\n\t"
    "flw fa7, 92(%0)\n\t"
    : /* no outputs */
    : "r" (float_data)
    : "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7"
  );
}

void
convert_more_to_bfloat16 (void)
{
  /* Convert more floats to bfloat16 */
  __asm__ __volatile__ (
    "fcvt.bf16.s fa0, fa0\n\t"
    "fcvt.bf16.s fa1, fa1\n\t"
    "fcvt.bf16.s fa2, fa2\n\t"
    "fcvt.bf16.s fa3, fa3\n\t"
    "fcvt.bf16.s fa4, fa4\n\t"
    "fcvt.bf16.s fa5, fa5\n\t"
    "fcvt.bf16.s fa6, fa6\n\t"
    "fcvt.bf16.s fa7, fa7\n\t"
    : /* no outputs */
    : /* no inputs */
    : "fa0", "fa1", "fa2", "fa3", "fa4", "fa5", "fa6", "fa7"
  );
}

int
main (int argc, char **argv)
{
  /* Load float values and convert to bfloat16 */
  load_float_to_fpu_regs ();
  convert_float_to_bfloat16 ();
  __asm__ ("nop"); /* first breakpoint here */

  /* Load more values and convert */
  load_more_floats ();
  convert_more_to_bfloat16 ();
  __asm__ ("nop"); /* second breakpoint here */

  return 0;
}
