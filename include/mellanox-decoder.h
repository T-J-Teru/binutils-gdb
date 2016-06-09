#ifndef MELLANOX_DECODER_H
#define MELLANOX_DECODER_H

#include <bfd.h>

/* Mellanox Instruction Decoder Support */

enum mellanox_operand_type {
  op_type_none,		/* No operand.  */
  op_type_core_reg,	/* Register operand.  */
  op_type_imm		/* Immediate operand.  */
};

struct mellanox_operand
{
  /* The type of this operand.  */
  enum mellanox_operand_type type;

  /* The value associated with this operand.  Is undefined for operands of
     type OP_TYPE_NONE, is the register number for OP_TYPE_CORE_REG, or is
     the immediate value for OP_TYPE_IMM.  */
  unsigned long operand;
};

struct mellanox_insn_decode
{
  /* Size of instruction in bytes.  */
  unsigned int insn_length;

  /* Size of LIMM in bytes.  This will either be 4 or 0.  */
  unsigned int limm_length;

  /* Details of 3 primary operands.  */
  struct mellanox_operand dst;
  struct mellanox_operand src1;
  struct mellanox_operand src2;

  /* Does this instruction set the flags?  */
  unsigned long set_flags;

  /* The disassembler output.  */
  char inst_disasm[512];
  char inst_ops_disasm[512];
};

#define MELLANOX_MAX_INSN_LENGTH 8

struct mellanox_insn
{
  /* Space for longest instruction.  */
  bfd_byte data [MELLANOX_MAX_INSN_LENGTH];

  /* Length of insn in bytes.  */
  unsigned int length;
};

/* Decode an instruction.  Return zero if the decode failed for some
   reason, otherwise return non-zero.  */
extern int mellanox_decode (struct mellanox_insn *, struct mellanox_insn_decode *);

#endif /* MELLANOX_DECODER_H */
