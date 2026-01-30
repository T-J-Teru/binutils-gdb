/* Simulator target description support.
   Copyright (C) 2026 Free Software Foundation, Inc.

This file is part of GDB, the GNU debugger.

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

#ifndef SIM_TDESC_H
#define SIM_TDESC_H

/* Data structures for describing registers to be converted to XML
   target descriptions.  Simulators populate these structures, and
   sim/common code converts them to XML.  */

/* Kind of composite type.  */

enum sim_tdesc_type_kind
{
  SIM_TDESC_TYPE_UNION,   /* Union of alternative representations.  */
  SIM_TDESC_TYPE_STRUCT,  /* Structure with named fields.  */
  SIM_TDESC_TYPE_FLAGS,   /* Flags register (bitfields only).  */
  SIM_TDESC_TYPE_VECTOR   /* Vector/array type.  */
};

/* Description of a field within a composite type (union, struct, flags).
   For non-bitfield fields (union, non-bitfield struct): use name and type.
   For bitfield fields (bitfield struct, flags): use name, start, end,
   and optionally type.  */

struct sim_tdesc_type_field
{
  /* Field name.  Required.  Can be empty string "" for filler bits.  */
  const char *name;

  /* Field type.  Required for union and non-bitfield struct.
     Optional for bitfield struct/flags (defaults to bool for 1-bit,
     unsigned integer otherwise).  NULL to use default.  */
  const char *type;

  /* For bitfields: start bit position (0 = LSB).  Set to -1 for
     non-bitfield fields.  */
  int start;

  /* For bitfields: end bit position (inclusive).  Must be >= start.
     Set to -1 for non-bitfield fields.  */
  int end;
};

/* Description of a composite type (union, struct, flags, or vector).  */

struct sim_tdesc_type
{
  /* Type identifier (used as the type name for registers).  */
  const char *id;

  /* Kind of type.  */
  enum sim_tdesc_type_kind kind;

  /* For bitfield struct/flags: total size in bytes.  Required when
     using bitfield fields.  Set to 0 for non-bitfield types.  */
  int size;

  /* For vector: element type.  NULL for other types.  */
  const char *element_type;

  /* For vector: number of elements.  0 for other types.  */
  int count;

  /* Array of fields (for union, struct, flags).  NULL for vector.  */
  const struct sim_tdesc_type_field *fields;

  /* Number of fields.  0 for vector.  */
  int num_fields;
};

/* Description of a single register.  */

struct sim_tdesc_reg
{
  /* Register name as it should appear in the target description.  */
  const char *name;

  /* Size of the register in bits.  */
  int bitsize;

  /* Register number.  If -1, the register number is assigned
     sequentially.  */
  int regnum;

  /* Type of the register.  Common types are "int" and "ieee_single",
     "ieee_double".  If NULL, defaults to "int".  */
  const char *type;
};

/* Description of a feature (group of registers).  */

struct sim_tdesc_feature
{
  /* Feature name, e.g., "org.gnu.gdb.riscv.cpu".  */
  const char *name;

  /* Array of types in this feature.  May be NULL if num_types is 0.
     Types must be defined before registers that reference them.  */
  const struct sim_tdesc_type *types;

  /* Number of types in the array.  */
  int num_types;

  /* Array of registers in this feature.  */
  const struct sim_tdesc_reg *regs;

  /* Number of registers in the array.  */
  int num_regs;
};

/* Complete target description.  */

struct sim_tdesc
{
  /* Architecture name, e.g., "riscv:rv64".  */
  const char *arch;

  /* Array of features.  */
  const struct sim_tdesc_feature *features;

  /* Number of features.  */
  int num_features;
};

#endif /* SIM_TDESC_H */
