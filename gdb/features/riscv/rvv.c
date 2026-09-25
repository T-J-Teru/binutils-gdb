/* Copyright (C) 2026 Free Software Foundation, Inc.

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

#include "gdbsupport/gdb_assert.h"
#include "gdbsupport/tdesc.h"
#include "gdb/riscv-regs.h"
#include <vector>

/* This file is NOT auto generated from xml.
   'create_feature_riscv_rvv' creates a RISCV Vector Extension feature.
   'vlenb' is a vector register size in bytes */

struct vector_field_type_info
{
  const char *element_type_name;
  const char *vector_type_name;
  int element_count;
};

static void
create_vector_register_type (tdesc_feature *feature,
			     std::vector<vector_field_type_info> &types_info,
			     const char *type_name)
{
  tdesc_type *element_type;

  for (auto &&type_info : types_info)
    {
      if (type_info.element_count == -1)
	continue;
      element_type = tdesc_named_type (feature, type_info.element_type_name);
      tdesc_create_vector (feature, type_info.vector_type_name, element_type,
			   type_info.element_count);
    }

  tdesc_type_with_fields *type_with_fields = tdesc_create_union (feature,
								 type_name);

  for (auto &&type_info : types_info)
    {
      if (type_info.element_count == -1)
	continue;
      element_type = tdesc_named_type (feature, type_info.vector_type_name);
      tdesc_add_field (type_with_fields, type_info.vector_type_name,
		       element_type);
    }
}

static int
create_feature_riscv_rvv (target_desc *result, int vlenb, int xlen)
{
  gdb_assert (result);
  gdb_assert (xlen == 4 || xlen == 8);
  gdb_assert (vlenb >= 4 && ((vlenb & (vlenb - 1)) == 0));

  int v_bitsize = 8 * vlenb;
  int x_bitsize = 8 * xlen;

  tdesc_feature *csr_feature = tdesc_create_feature (result,
						     "org.gnu.gdb.riscv.csr");
  tdesc_create_reg (csr_feature, "vstart", RISCV_CSR_VSTART_REGNUM, 1, NULL,
		    x_bitsize, "int");
  tdesc_create_reg (csr_feature, "vcsr", RISCV_CSR_VCSR_REGNUM, 1, NULL,
		    x_bitsize, "int");
  tdesc_create_reg (csr_feature, "vl", RISCV_CSR_VL_REGNUM, 1, NULL, x_bitsize,
		    "int");
  tdesc_create_reg (csr_feature, "vtype", RISCV_CSR_VTYPE_REGNUM, 1, NULL,
		    x_bitsize, "int");
  tdesc_create_reg (csr_feature, "vlenb", RISCV_CSR_VLENB_REGNUM, 1, NULL,
		    x_bitsize, "int");

  tdesc_feature *vector_feature
    = tdesc_create_feature (result, "org.gnu.gdb.riscv.vector");

  std::vector<vector_field_type_info> elements_types_info
    = { { "int8", "i8", vlenb },
	{ "int16", "i16", vlenb / 2 },
	{ "int32", "i32", vlenb / 4 },
	{ "int64", "i64", (vlenb >= 8) ? vlenb / 8 : -1 },
	{ "ieee_half", "half", vlenb / 2 },
	{ "ieee_single", "f32", vlenb / 4 },
	{ "ieee_double", "f64", (vlenb >= 8) ? vlenb / 8 : -1 } };

  create_vector_register_type (vector_feature, elements_types_info, "rvv");

  int regnum = RISCV_V0_REGNUM;

  constexpr const char *vec_reg_names[]
    = { "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",
	"v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
	"v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",
	"v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31" };

  for (int i = 0; i < 32; ++i)
    tdesc_create_reg (vector_feature, vec_reg_names[i], regnum++, 1, NULL,
		      v_bitsize, "rvv");

  return regnum;
}
