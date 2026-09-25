# Copyright 2026 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

import os
import itertools
import re
from pathlib import Path
from enum import Enum
from jinja2 import Environment, FileSystemLoader

FILE = Path(__file__).name
TEST_DIR = Path(__file__).resolve().parent
JINJA_TEMPLATE_FILE = "riscv-vector-abi-full-generate-template.txt"

WORK_DIR = os.getenv("WORK_DIR")
TEST_NAME = os.getenv("TEST_NAME")
HAS_ZVFH = os.getenv("HAS_ZVFH") == "1"


class ElemType(str, Enum):
    INT = "int"
    UINT = "uint"
    FLOAT = "float"


class InstrType(str, Enum):
    VADD = "vadd"
    VMV = "vmv"
    VGET = "vget"
    VSET = "vset"
    VSETVLMAX = "vlmax"


class InstructionTemplate:
    vadd_instr_templates = {
        ElemType.INT: "__riscv_vadd_vv_i{suffix1}",
        ElemType.UINT: "__riscv_vadd_vv_u{suffix1}",
        ElemType.FLOAT: "__riscv_vfadd_vv_f{suffix1}",
    }

    vmv_instr_templates = {
        ElemType.INT: "__riscv_vmv_v_x_i{suffix1}",
        ElemType.UINT: "__riscv_vmv_v_x_u{suffix1}",
        ElemType.FLOAT: "__riscv_vfmv_v_f_f{suffix1}",
    }

    vget_instr_templates = {
        ElemType.INT: "__riscv_vget_v_i{suffix1}_i{suffix2}",
        ElemType.UINT: "__riscv_vget_v_u{suffix1}_u{suffix2}",
        ElemType.FLOAT: "__riscv_vget_v_f{suffix1}_f{suffix2}",
    }

    vset_instr_templates = {
        ElemType.INT: "__riscv_vset_v_i{suffix1}_i{suffix2}",
        ElemType.UINT: "__riscv_vset_v_u{suffix1}_u{suffix2}",
        ElemType.FLOAT: "__riscv_vset_v_f{suffix1}_f{suffix2}",
    }

    vsetvlmax_template = {k: "__riscv_vsetvlmax_e{suffix1}" for k in ElemType}

    templates = {
        InstrType.VADD: vadd_instr_templates,
        InstrType.VMV: vmv_instr_templates,
        InstrType.VGET: vget_instr_templates,
        InstrType.VSET: vset_instr_templates,
        InstrType.VSETVLMAX: vsetvlmax_template,
    }

    def get(self, elem_type: ElemType, instr_type: InstrType):
        return self.templates[instr_type][elem_type]


def generate(directory: Path, test_name: Path):
    instr_templates = InstructionTemplate()

    env = Environment(loader=FileSystemLoader(str(TEST_DIR)))
    tpl = env.get_template(str(JINJA_TEMPLATE_FILE))

    counter_vars = itertools.count(0)
    counter_values = itertools.cycle(range(0, 64, 1))
    counter_break_idx = itertools.count(2)

    main_file = Path(f"{test_name}.c")
    main_file_path = directory / main_file

    test_script = Path(f"{test_name}.exp")
    test_script_path = directory / test_script

    if not os.path.exists(main_file_path):
        os.mknod(main_file_path)

    if not os.path.exists(test_script_path):
        os.mknod(test_script_path)

    main_header = tpl.module.main_header(FILE)

    with open(main_file_path, "w") as f:
        f.write(main_header)

    main_tail = tpl.module.main_tail_start()

    expect_header = tpl.module.expect_header(FILE)

    with open(test_script_path, "w") as f:
        f.write(expect_header)

    # int, uint, float

    # fmt: off
    vint_types = [
        # 8-bit
        "vint8mf8_t", "vint8mf4_t", "vint8mf2_t", "vint8m1_t", "vint8m2_t", "vint8m4_t", "vint8m8_t",

        # 16-bit
        "vint16mf4_t", "vint16mf2_t", "vint16m1_t", "vint16m2_t", "vint16m4_t", "vint16m8_t",

        # 32-bit
        "vint32mf2_t", "vint32m1_t", "vint32m2_t", "vint32m4_t", "vint32m8_t",

        # 64-bit
        "vint64m1_t", "vint64m2_t", "vint64m4_t", "vint64m8_t",
    ]

    vuint_types = [_.replace("int", "uint") for _ in vint_types]

    vfloat_types = []
    if HAS_ZVFH:
        vfloat_types += [
            # SEW = 16 (half-precision)
            "vfloat16mf4_t", "vfloat16mf2_t", "vfloat16m1_t", "vfloat16m2_t", "vfloat16m4_t", "vfloat16m8_t",
        ]

    vfloat_types += [
        # SEW = 32 (single-precision)
        "vfloat32mf2_t", "vfloat32m1_t", "vfloat32m2_t", "vfloat32m4_t", "vfloat32m8_t",

        # SEW = 64 (double-precision)
        "vfloat64m1_t", "vfloat64m2_t", "vfloat64m4_t", "vfloat64m8_t",
    ]
    # fmt: on

    for type_name in vint_types + vuint_types + vfloat_types:
        m = re.match(r"v(int|uint|float)(8|16|32|64)(m|mf)(1|2|4|8)_t", type_name)
        if not m:
            raise RuntimeError("wrong type")

        elem_type = ElemType(m.group(1))
        small_suffix = "".join(m.group(2, 3, 4))  # 16m2

        func_name = tpl.module.func_name_template(type_name)
        vsetvlmax = instr_templates.get(elem_type, InstrType.VSETVLMAX).format(
            suffix1=small_suffix
        )
        vadd_name = instr_templates.get(elem_type, InstrType.VADD).format(
            suffix1=small_suffix
        )
        vmv_name = instr_templates.get(elem_type, InstrType.VMV).format(
            suffix1=small_suffix
        )

        var_idx = next(counter_vars)
        var_val = next(counter_values)
        res_val = 2 * var_val

        new_line = tpl.module.func_template(
            type_name,
            vadd_name,
            func_name,
            vsetvlmax,
        )

        with open(main_file_path, "a") as f:
            f.write(new_line)

        main_tail += tpl.module.main_entry_template(
            type_name,
            var_idx,
            vmv_name,
            var_val,
            func_name,
            vsetvlmax,
        )

        break_idx = next(counter_break_idx)
        test_command = tpl.module.test_entry_template(
            main_file,
            type_name,
            break_idx,
            var_idx,
            var_val,
            res_val,
            func_name,
        )
        with open(test_script_path, "a") as f:
            f.write(test_command)

    # tuple int

    # fmt: off
    vint_tuple_types = [
        # LMUL = mf8
        "vint8mf8x2_t", "vint8mf8x3_t", "vint8mf8x4_t", "vint8mf8x5_t", "vint8mf8x6_t", "vint8mf8x7_t", "vint8mf8x8_t",

        # LMUL = mf4
        "vint8mf4x2_t", "vint8mf4x3_t", "vint8mf4x4_t", "vint8mf4x5_t", "vint8mf4x6_t", "vint8mf4x7_t", "vint8mf4x8_t",
        "vint16mf4x2_t", "vint16mf4x3_t", "vint16mf4x4_t", "vint16mf4x5_t", "vint16mf4x6_t", "vint16mf4x7_t", "vint16mf4x8_t",

        # LMUL = mf2
        "vint8mf2x2_t", "vint8mf2x3_t", "vint8mf2x4_t", "vint8mf2x5_t", "vint8mf2x6_t", "vint8mf2x7_t", "vint8mf2x8_t",
        "vint16mf2x2_t", "vint16mf2x3_t", "vint16mf2x4_t", "vint16mf2x5_t", "vint16mf2x6_t", "vint16mf2x7_t", "vint16mf2x8_t",
        "vint32mf2x2_t", "vint32mf2x3_t", "vint32mf2x4_t", "vint32mf2x5_t", "vint32mf2x6_t", "vint32mf2x7_t", "vint32mf2x8_t",

        # LMUL = m1
        "vint8m1x2_t", "vint8m1x3_t", "vint8m1x4_t", "vint8m1x5_t", "vint8m1x6_t", "vint8m1x7_t", "vint8m1x8_t",
        "vint16m1x2_t", "vint16m1x3_t", "vint16m1x4_t", "vint16m1x5_t", "vint16m1x6_t", "vint16m1x7_t", "vint16m1x8_t",
        "vint32m1x2_t", "vint32m1x3_t", "vint32m1x4_t", "vint32m1x5_t", "vint32m1x6_t", "vint32m1x7_t", "vint32m1x8_t",
        "vint64m1x2_t", "vint64m1x3_t", "vint64m1x4_t", "vint64m1x5_t", "vint64m1x6_t", "vint64m1x7_t", "vint64m1x8_t",

        # LMUL = m2
        "vint8m2x2_t", "vint8m2x3_t", "vint8m2x4_t",
        "vint16m2x2_t", "vint16m2x3_t", "vint16m2x4_t",
        "vint32m2x2_t", "vint32m2x3_t", "vint32m2x4_t",
        "vint64m2x2_t", "vint64m2x3_t", "vint64m2x4_t",

        # LMUL = m4
        "vint8m4x2_t",
        "vint16m4x2_t",
        "vint32m4x2_t",
        "vint64m4x2_t",
    ]

    vuint_tuple_types = [_.replace("int", "uint") for _ in vint_tuple_types]

    vfloat_tuple_types = []
    if HAS_ZVFH:
        vfloat_tuple_types += [
            # vfloat16
            "vfloat16mf4x2_t", "vfloat16mf4x3_t", "vfloat16mf4x4_t", "vfloat16mf4x5_t",
            "vfloat16mf4x6_t", "vfloat16mf4x7_t", "vfloat16mf4x8_t",
            "vfloat16mf2x2_t", "vfloat16mf2x3_t", "vfloat16mf2x4_t", "vfloat16mf2x5_t",
            "vfloat16mf2x6_t", "vfloat16mf2x7_t", "vfloat16mf2x8_t",
            "vfloat16m1x2_t", "vfloat16m1x3_t", "vfloat16m1x4_t", "vfloat16m1x5_t",
            "vfloat16m1x6_t", "vfloat16m1x7_t", "vfloat16m1x8_t",
            "vfloat16m2x2_t", "vfloat16m2x3_t", "vfloat16m2x4_t",
            "vfloat16m4x2_t",
        ]

    vfloat_tuple_types += [
        # LMUL = mf2 (1/2)
        "vfloat32mf2x2_t", "vfloat32mf2x3_t", "vfloat32mf2x4_t", "vfloat32mf2x5_t",
        "vfloat32mf2x6_t", "vfloat32mf2x7_t", "vfloat32mf2x8_t",

        # LMUL = m1 (1)
        "vfloat32m1x2_t", "vfloat32m1x3_t", "vfloat32m1x4_t", "vfloat32m1x5_t",
        "vfloat32m1x6_t", "vfloat32m1x7_t", "vfloat32m1x8_t",
        "vfloat64m1x2_t", "vfloat64m1x3_t", "vfloat64m1x4_t", "vfloat64m1x5_t",
        "vfloat64m1x6_t", "vfloat64m1x7_t", "vfloat64m1x8_t",

        # LMUL = m2 (2)
        "vfloat32m2x2_t", "vfloat32m2x3_t", "vfloat32m2x4_t",
        "vfloat64m2x2_t", "vfloat64m2x3_t", "vfloat64m2x4_t",

        # LMUL = m4 (4)
        "vfloat32m4x2_t",
        "vfloat64m4x2_t",
    ]
    # fmt: on

    def get_tuple_template(nfields: int) -> str:
        tuple_template = tpl.module.tuple_func_template_start()
        for i in range(nfields):
            tuple_template += tpl.module.tuple_func_template_entry(i)
        tuple_template += tpl.module.tuple_func_template_end()
        return tuple_template

    def get_main_tuple_template(nfields: int) -> str:
        main_tuple_template = tpl.module.tuple_main_entry_template_start()
        for i in range(nfields):
            main_tuple_template += tpl.module.tuple_main_entry_template_entry(i)
        main_tuple_template += tpl.module.tuple_main_entry_template_end()
        return main_tuple_template

    def get_test_tuple_template(nfields: int) -> str:
        test_tuple_template = tpl.module.tuple_test_template_start(main_file)
        for i in range(nfields):
            test_tuple_template += tpl.module.tuple_test_template_entry_first(i)
        test_tuple_template += tpl.module.tuple_test_template_entry_middle()
        for i in range(nfields):
            test_tuple_template += tpl.module.tuple_test_template_entry_second(i)
        test_tuple_template += tpl.module.tuple_test_template_end(nfields)
        return test_tuple_template

    for type_name in vint_tuple_types + vuint_tuple_types + vfloat_tuple_types:
        m = re.match(
            r"v(int|uint|float)(8|16|32|64)(m|mf)(1|2|4|8)(x)([2-8])_t",
            type_name,
        )
        if not m:
            raise RuntimeError("wrong type")

        elem_type = ElemType(m.group(1))
        nfields = int(m.group(6))
        short = type_name[:-4] + "_t"
        big_suffix = "".join(m.group(2, 3, 4, 5, 6))  # 16m2x3
        small_suffix = "".join(m.group(2, 3, 4))  # 16m2

        func_name = tpl.module.func_name_template(type_name)
        vsetvlmax = instr_templates.get(elem_type, InstrType.VSETVLMAX).format(
            suffix1=small_suffix
        )
        vget_name = instr_templates.get(elem_type, InstrType.VGET).format(
            suffix1=big_suffix, suffix2=small_suffix
        )
        vadd_name = instr_templates.get(elem_type, InstrType.VADD).format(
            suffix1=small_suffix
        )
        vset_name = instr_templates.get(elem_type, InstrType.VSET).format(
            suffix1=small_suffix, suffix2=big_suffix
        )
        vmv_name = instr_templates.get(elem_type, InstrType.VMV).format(
            suffix1=small_suffix
        )

        var_idx = next(counter_vars)
        var_values = [val for _, val in zip(range(nfields), counter_values)]
        res_values = [2 * val for val in var_values]

        string = get_tuple_template(nfields).format(
            type_name=type_name,
            short_type_name=short,
            vget_name=vget_name,
            vadd_name=vadd_name,
            vset_name=vset_name,
            func_name=func_name,
            vsetvlmax=vsetvlmax,
        )

        with open(main_file_path, "a") as f:
            f.write(string)

        main_tail += get_main_tuple_template(nfields).format(
            vsetvlmax=vsetvlmax,
            short_type_name=short,
            var_idx=var_idx,
            small_suffix=small_suffix,
            type_name=type_name,
            vset_name=vset_name,
            func_name=func_name,
            vmv_name=vmv_name,
            var_values=var_values,
        )

        break_idx = next(counter_break_idx)
        test_command = get_test_tuple_template(nfields).format(
            type_name=type_name,
            break_idx=break_idx,
            var_idx=var_idx,
            var_values=var_values,
            res_values=res_values,
            func_name=func_name,
        )

        with open(test_script_path, "a") as f:
            f.write(test_command)

    with open(main_file_path, "a") as f:
        f.write(main_tail)
        f.write("\n  return;\n}\n")
        f.write("\nint main () {test();}\n")


generate(WORK_DIR, TEST_NAME)
