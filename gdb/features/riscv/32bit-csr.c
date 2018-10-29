/* THIS FILE IS GENERATED.  -*- buffer-read-only: t -*- vi:set ro:
  Original: 32bit-csr.xml */

#include "common/tdesc.h"

static int
create_feature_riscv_32bit_csr (struct target_desc *result, long regnum)
{
  struct tdesc_feature *feature;

  feature = tdesc_create_feature (result, "org.gnu.gdb.riscv.csr");
  return regnum;
}
