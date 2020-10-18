/* Fortran language support routines for GDB, the GNU debugger.

   Copyright (C) 1993-2020 Free Software Foundation, Inc.

   Contributed by Motorola.  Adapted from the C parser by Farooq Butt
   (fmbutt@engage.sps.mot.com).

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

#include "defs.h"
#include "symtab.h"
#include "gdbtypes.h"
#include "expression.h"
#include "parser-defs.h"
#include "language.h"
#include "varobj.h"
#include "gdbcore.h"
#include "f-lang.h"
#include "valprint.h"
#include "value.h"
#include "cp-support.h"
#include "charset.h"
#include "c-lang.h"
#include "target-float.h"
#include "gdbarch.h"
#include "gdbcmd.h"

#include <math.h>

/* Local functions */

static value *fortran_prepare_argument (struct expression *exp, int *pos,
					int arg_num, bool is_internal_call_p,
					struct type *func_type,
					enum noside noside);

/* Return the encoding that should be used for the character type
   TYPE.  */

static const char *
f_get_encoding (struct type *type)
{
  const char *encoding;

  switch (TYPE_LENGTH (type))
    {
    case 1:
      encoding = target_charset (get_type_arch (type));
      break;
    case 4:
      if (type_byte_order (type) == BFD_ENDIAN_BIG)
	encoding = "UTF-32BE";
      else
	encoding = "UTF-32LE";
      break;

    default:
      error (_("unrecognized character type"));
    }

  return encoding;
}



/* Table of operators and their precedences for printing expressions.  */

static const struct op_print f_op_print_tab[] =
{
  {"+", BINOP_ADD, PREC_ADD, 0},
  {"+", UNOP_PLUS, PREC_PREFIX, 0},
  {"-", BINOP_SUB, PREC_ADD, 0},
  {"-", UNOP_NEG, PREC_PREFIX, 0},
  {"*", BINOP_MUL, PREC_MUL, 0},
  {"/", BINOP_DIV, PREC_MUL, 0},
  {"DIV", BINOP_INTDIV, PREC_MUL, 0},
  {"MOD", BINOP_REM, PREC_MUL, 0},
  {"=", BINOP_ASSIGN, PREC_ASSIGN, 1},
  {".OR.", BINOP_LOGICAL_OR, PREC_LOGICAL_OR, 0},
  {".AND.", BINOP_LOGICAL_AND, PREC_LOGICAL_AND, 0},
  {".NOT.", UNOP_LOGICAL_NOT, PREC_PREFIX, 0},
  {".EQ.", BINOP_EQUAL, PREC_EQUAL, 0},
  {".NE.", BINOP_NOTEQUAL, PREC_EQUAL, 0},
  {".LE.", BINOP_LEQ, PREC_ORDER, 0},
  {".GE.", BINOP_GEQ, PREC_ORDER, 0},
  {".GT.", BINOP_GTR, PREC_ORDER, 0},
  {".LT.", BINOP_LESS, PREC_ORDER, 0},
  {"**", UNOP_IND, PREC_PREFIX, 0},
  {"@", BINOP_REPEAT, PREC_REPEAT, 0},
  {NULL, OP_NULL, PREC_REPEAT, 0}
};

enum f_primitive_types {
  f_primitive_type_character,
  f_primitive_type_logical,
  f_primitive_type_logical_s1,
  f_primitive_type_logical_s2,
  f_primitive_type_logical_s8,
  f_primitive_type_integer,
  f_primitive_type_integer_s2,
  f_primitive_type_real,
  f_primitive_type_real_s8,
  f_primitive_type_real_s16,
  f_primitive_type_complex_s8,
  f_primitive_type_complex_s16,
  f_primitive_type_void,
  nr_f_primitive_types
};

/* Called from fortran_value_subarray to take a slice of an array or a
   string.  ARRAY is the array or string to be accessed.  EXP, POS, and
   NOSIDE are as for evaluate_subexp_standard.  Return a value that is a
   slice of the array.  */

static struct value *
value_f90_subarray (struct value *array,
		    struct expression *exp, int *pos, enum noside noside)
{
  int pc = (*pos) + 1;
  LONGEST low_bound, high_bound;
  struct type *range = check_typedef (value_type (array)->index_type ());
  enum range_type range_type
    = (enum range_type) longest_to_int (exp->elts[pc].longconst);

  *pos += 3;

  if (range_type == LOW_BOUND_DEFAULT || range_type == BOTH_BOUND_DEFAULT)
    low_bound = range->bounds ()->low.const_val ();
  else
    low_bound = value_as_long (evaluate_subexp (nullptr, exp, pos, noside));

  if (range_type == HIGH_BOUND_DEFAULT || range_type == BOTH_BOUND_DEFAULT)
    high_bound = range->bounds ()->high.const_val ();
  else
    high_bound = value_as_long (evaluate_subexp (nullptr, exp, pos, noside));

  return value_slice (array, low_bound, high_bound - low_bound + 1);
}

/* Helper for skipping all the arguments in an undetermined argument list.
   This function was designed for use in the OP_F77_UNDETERMINED_ARGLIST
   case of evaluate_subexp_standard as multiple, but not all, code paths
   require a generic skip.  */

static void
skip_undetermined_arglist (int nargs, struct expression *exp, int *pos,
			   enum noside noside)
{
  for (int i = 0; i < nargs; ++i)
    evaluate_subexp (nullptr, exp, pos, noside);
}

/* Return the number of dimensions for a Fortran array or string.  */

int
calc_f77_array_dims (struct type *array_type)
{
  int ndimen = 1;
  struct type *tmp_type;

  if ((array_type->code () == TYPE_CODE_STRING))
    return 1;

  if ((array_type->code () != TYPE_CODE_ARRAY))
    error (_("Can't get dimensions for a non-array type"));

  tmp_type = array_type;

  while ((tmp_type = TYPE_TARGET_TYPE (tmp_type)))
    {
      if (tmp_type->code () == TYPE_CODE_ARRAY)
	++ndimen;
    }
  return ndimen;
}

/* Called from evaluate_subexp_standard to perform array indexing, and
   sub-range extraction, for Fortran.  As well as arrays this function
   also handles strings as they can be treated like arrays of characters.
   ARRAY is the array or string being accessed.  EXP, POS, and NOSIDE are
   as for evaluate_subexp_standard, and NARGS is the number of arguments
   in this access (e.g. 'array (1,2,3)' would be NARGS 3).  */

static struct value *
fortran_value_subarray (struct value *array, struct expression *exp,
			int *pos, int nargs, enum noside noside)
{
  if (exp->elts[*pos].opcode == OP_RANGE)
    return value_f90_subarray (array, exp, pos, noside);

  if (noside == EVAL_SKIP)
    {
      skip_undetermined_arglist (nargs, exp, pos, noside);
      /* Return the dummy value with the correct type.  */
      return array;
    }

  LONGEST subscript_array[MAX_FORTRAN_DIMS];
  int ndimensions = 1;
  struct type *type = check_typedef (value_type (array));

  if (nargs > MAX_FORTRAN_DIMS)
    error (_("Too many subscripts for F77 (%d Max)"), MAX_FORTRAN_DIMS);

  ndimensions = calc_f77_array_dims (type);

  if (nargs != ndimensions)
    error (_("Wrong number of subscripts"));

  gdb_assert (nargs > 0);

  /* Now that we know we have a legal array subscript expression let us
     actually find out where this element exists in the array.  */

  /* Take array indices left to right.  */
  for (int i = 0; i < nargs; i++)
    {
      /* Evaluate each subscript; it must be a legal integer in F77.  */
      value *arg2 = evaluate_subexp_with_coercion (exp, pos, noside);

      /* Fill in the subscript array.  */
      subscript_array[i] = value_as_long (arg2);
    }

  /* Internal type of array is arranged right to left.  */
  for (int i = nargs; i > 0; i--)
    {
      struct type *array_type = check_typedef (value_type (array));
      LONGEST index = subscript_array[i - 1];

      array = value_subscripted_rvalue (array, index,
					f77_get_lowerbound (array_type));
    }

  return array;
}

/* Special expression evaluation cases for Fortran.  */

static struct value *
evaluate_subexp_f (struct type *expect_type, struct expression *exp,
		   int *pos, enum noside noside)
{
  struct value *arg1 = NULL, *arg2 = NULL;
  enum exp_opcode op;
  int pc;
  struct type *type;

  pc = *pos;
  *pos += 1;
  op = exp->elts[pc].opcode;

  switch (op)
    {
    default:
      *pos -= 1;
      return evaluate_subexp_standard (expect_type, exp, pos, noside);

    case UNOP_ABS:
      arg1 = evaluate_subexp (nullptr, exp, pos, noside);
      if (noside == EVAL_SKIP)
	return eval_skip_value (exp);
      type = value_type (arg1);
      switch (type->code ())
	{
	case TYPE_CODE_FLT:
	  {
	    double d
	      = fabs (target_float_to_host_double (value_contents (arg1),
						   value_type (arg1)));
	    return value_from_host_double (type, d);
	  }
	case TYPE_CODE_INT:
	  {
	    LONGEST l = value_as_long (arg1);
	    l = llabs (l);
	    return value_from_longest (type, l);
	  }
	}
      error (_("ABS of type %s not supported"), TYPE_SAFE_NAME (type));

    case BINOP_MOD:
      arg1 = evaluate_subexp (nullptr, exp, pos, noside);
      arg2 = evaluate_subexp (value_type (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	return eval_skip_value (exp);
      type = value_type (arg1);
      if (type->code () != value_type (arg2)->code ())
	error (_("non-matching types for parameters to MOD ()"));
      switch (type->code ())
	{
	case TYPE_CODE_FLT:
	  {
	    double d1
	      = target_float_to_host_double (value_contents (arg1),
					     value_type (arg1));
	    double d2
	      = target_float_to_host_double (value_contents (arg2),
					     value_type (arg2));
	    double d3 = fmod (d1, d2);
	    return value_from_host_double (type, d3);
	  }
	case TYPE_CODE_INT:
	  {
	    LONGEST v1 = value_as_long (arg1);
	    LONGEST v2 = value_as_long (arg2);
	    if (v2 == 0)
	      error (_("calling MOD (N, 0) is undefined"));
	    LONGEST v3 = v1 - (v1 / v2) * v2;
	    return value_from_longest (value_type (arg1), v3);
	  }
	}
      error (_("MOD of type %s not supported"), TYPE_SAFE_NAME (type));

    case UNOP_FORTRAN_CEILING:
      {
	arg1 = evaluate_subexp (nullptr, exp, pos, noside);
	if (noside == EVAL_SKIP)
	  return eval_skip_value (exp);
	type = value_type (arg1);
	if (type->code () != TYPE_CODE_FLT)
	  error (_("argument to CEILING must be of type float"));
	double val
	  = target_float_to_host_double (value_contents (arg1),
					 value_type (arg1));
	val = ceil (val);
	return value_from_host_double (type, val);
      }

    case UNOP_FORTRAN_FLOOR:
      {
	arg1 = evaluate_subexp (nullptr, exp, pos, noside);
	if (noside == EVAL_SKIP)
	  return eval_skip_value (exp);
	type = value_type (arg1);
	if (type->code () != TYPE_CODE_FLT)
	  error (_("argument to FLOOR must be of type float"));
	double val
	  = target_float_to_host_double (value_contents (arg1),
					 value_type (arg1));
	val = floor (val);
	return value_from_host_double (type, val);
      }

    case BINOP_FORTRAN_MODULO:
      {
	arg1 = evaluate_subexp (nullptr, exp, pos, noside);
	arg2 = evaluate_subexp (value_type (arg1), exp, pos, noside);
	if (noside == EVAL_SKIP)
	  return eval_skip_value (exp);
	type = value_type (arg1);
	if (type->code () != value_type (arg2)->code ())
	  error (_("non-matching types for parameters to MODULO ()"));
        /* MODULO(A, P) = A - FLOOR (A / P) * P */
	switch (type->code ())
	  {
	  case TYPE_CODE_INT:
	    {
	      LONGEST a = value_as_long (arg1);
	      LONGEST p = value_as_long (arg2);
	      LONGEST result = a - (a / p) * p;
	      if (result != 0 && (a < 0) != (p < 0))
		result += p;
	      return value_from_longest (value_type (arg1), result);
	    }
	  case TYPE_CODE_FLT:
	    {
	      double a
		= target_float_to_host_double (value_contents (arg1),
					       value_type (arg1));
	      double p
		= target_float_to_host_double (value_contents (arg2),
					       value_type (arg2));
	      double result = fmod (a, p);
	      if (result != 0 && (a < 0.0) != (p < 0.0))
		result += p;
	      return value_from_host_double (type, result);
	    }
	  }
	error (_("MODULO of type %s not supported"), TYPE_SAFE_NAME (type));
      }

    case BINOP_FORTRAN_CMPLX:
      arg1 = evaluate_subexp (nullptr, exp, pos, noside);
      arg2 = evaluate_subexp (value_type (arg1), exp, pos, noside);
      if (noside == EVAL_SKIP)
	return eval_skip_value (exp);
      type = builtin_f_type(exp->gdbarch)->builtin_complex_s16;
      return value_literal_complex (arg1, arg2, type);

    case UNOP_FORTRAN_KIND:
      arg1 = evaluate_subexp (NULL, exp, pos, EVAL_AVOID_SIDE_EFFECTS);
      type = value_type (arg1);

      switch (type->code ())
        {
          case TYPE_CODE_STRUCT:
          case TYPE_CODE_UNION:
          case TYPE_CODE_MODULE:
          case TYPE_CODE_FUNC:
            error (_("argument to kind must be an intrinsic type"));
        }

      if (!TYPE_TARGET_TYPE (type))
        return value_from_longest (builtin_type (exp->gdbarch)->builtin_int,
				   TYPE_LENGTH (type));
      return value_from_longest (builtin_type (exp->gdbarch)->builtin_int,
				 TYPE_LENGTH (TYPE_TARGET_TYPE (type)));


    case OP_F77_UNDETERMINED_ARGLIST:
      /* Remember that in F77, functions, substring ops and array subscript
         operations cannot be disambiguated at parse time.  We have made
         all array subscript operations, substring operations as well as
         function calls come here and we now have to discover what the heck
         this thing actually was.  If it is a function, we process just as
         if we got an OP_FUNCALL.  */
      int nargs = longest_to_int (exp->elts[pc + 1].longconst);
      (*pos) += 2;

      /* First determine the type code we are dealing with.  */
      arg1 = evaluate_subexp (nullptr, exp, pos, noside);
      type = check_typedef (value_type (arg1));
      enum type_code code = type->code ();

      if (code == TYPE_CODE_PTR)
	{
	  /* Fortran always passes variable to subroutines as pointer.
	     So we need to look into its target type to see if it is
	     array, string or function.  If it is, we need to switch
	     to the target value the original one points to.  */
	  struct type *target_type = check_typedef (TYPE_TARGET_TYPE (type));

	  if (target_type->code () == TYPE_CODE_ARRAY
	      || target_type->code () == TYPE_CODE_STRING
	      || target_type->code () == TYPE_CODE_FUNC)
	    {
	      arg1 = value_ind (arg1);
	      type = check_typedef (value_type (arg1));
	      code = type->code ();
	    }
	}

      switch (code)
	{
	case TYPE_CODE_ARRAY:
	case TYPE_CODE_STRING:
	  return fortran_value_subarray (arg1, exp, pos, nargs, noside);

	case TYPE_CODE_PTR:
	case TYPE_CODE_FUNC:
	case TYPE_CODE_INTERNAL_FUNCTION:
	  {
	    /* It's a function call.  Allocate arg vector, including
	    space for the function to be called in argvec[0] and a
	    termination NULL.  */
	    struct value **argvec = (struct value **)
	      alloca (sizeof (struct value *) * (nargs + 2));
	    argvec[0] = arg1;
	    int tem = 1;
	    for (; tem <= nargs; tem++)
	      {
		bool is_internal_func = (code == TYPE_CODE_INTERNAL_FUNCTION);
		argvec[tem]
		  = fortran_prepare_argument (exp, pos, (tem - 1),
					      is_internal_func,
					      value_type (arg1), noside);
	      }
	    argvec[tem] = 0;	/* signal end of arglist */
	    if (noside == EVAL_SKIP)
	      return eval_skip_value (exp);
	    return evaluate_subexp_do_call (exp, noside, nargs, argvec, NULL,
					    expect_type);
	  }

	default:
	  error (_("Cannot perform substring on this type"));
	}
    }

  /* Should be unreachable.  */
  return nullptr;
}

/* Special expression lengths for Fortran.  */

static void
operator_length_f (const struct expression *exp, int pc, int *oplenp,
		   int *argsp)
{
  int oplen = 1;
  int args = 0;

  switch (exp->elts[pc - 1].opcode)
    {
    default:
      operator_length_standard (exp, pc, oplenp, argsp);
      return;

    case UNOP_FORTRAN_KIND:
    case UNOP_FORTRAN_FLOOR:
    case UNOP_FORTRAN_CEILING:
      oplen = 1;
      args = 1;
      break;

    case BINOP_FORTRAN_CMPLX:
    case BINOP_FORTRAN_MODULO:
      oplen = 1;
      args = 2;
      break;

    case OP_F77_UNDETERMINED_ARGLIST:
      oplen = 3;
      args = 1 + longest_to_int (exp->elts[pc - 2].longconst);
      break;
    }

  *oplenp = oplen;
  *argsp = args;
}

/* Helper for PRINT_SUBEXP_F.  Arguments are as for PRINT_SUBEXP_F, except
   the extra argument NAME which is the text that should be printed as the
   name of this operation.  */

static void
print_unop_subexp_f (struct expression *exp, int *pos,
		     struct ui_file *stream, enum precedence prec,
		     const char *name)
{
  (*pos)++;
  fprintf_filtered (stream, "%s(", name);
  print_subexp (exp, pos, stream, PREC_SUFFIX);
  fputs_filtered (")", stream);
}

/* Helper for PRINT_SUBEXP_F.  Arguments are as for PRINT_SUBEXP_F, except
   the extra argument NAME which is the text that should be printed as the
   name of this operation.  */

static void
print_binop_subexp_f (struct expression *exp, int *pos,
		      struct ui_file *stream, enum precedence prec,
		      const char *name)
{
  (*pos)++;
  fprintf_filtered (stream, "%s(", name);
  print_subexp (exp, pos, stream, PREC_SUFFIX);
  fputs_filtered (",", stream);
  print_subexp (exp, pos, stream, PREC_SUFFIX);
  fputs_filtered (")", stream);
}

/* Special expression printing for Fortran.  */

static void
print_subexp_f (struct expression *exp, int *pos,
		struct ui_file *stream, enum precedence prec)
{
  int pc = *pos;
  enum exp_opcode op = exp->elts[pc].opcode;

  switch (op)
    {
    default:
      print_subexp_standard (exp, pos, stream, prec);
      return;

    case UNOP_FORTRAN_KIND:
      print_unop_subexp_f (exp, pos, stream, prec, "KIND");
      return;

    case UNOP_FORTRAN_FLOOR:
      print_unop_subexp_f (exp, pos, stream, prec, "FLOOR");
      return;

    case UNOP_FORTRAN_CEILING:
      print_unop_subexp_f (exp, pos, stream, prec, "CEILING");
      return;

    case BINOP_FORTRAN_CMPLX:
      print_binop_subexp_f (exp, pos, stream, prec, "CMPLX");
      return;

    case BINOP_FORTRAN_MODULO:
      print_binop_subexp_f (exp, pos, stream, prec, "MODULO");
      return;

    case OP_F77_UNDETERMINED_ARGLIST:
      print_subexp_funcall (exp, pos, stream);
      return;
    }
}

/* Special expression names for Fortran.  */

static const char *
op_name_f (enum exp_opcode opcode)
{
  switch (opcode)
    {
    default:
      return op_name_standard (opcode);

#define OP(name)	\
    case name:		\
      return #name ;
#include "fortran-operator.def"
#undef OP
    }
}

/* Special expression dumping for Fortran.  */

static int
dump_subexp_body_f (struct expression *exp,
		    struct ui_file *stream, int elt)
{
  int opcode = exp->elts[elt].opcode;
  int oplen, nargs, i;

  switch (opcode)
    {
    default:
      return dump_subexp_body_standard (exp, stream, elt);

    case UNOP_FORTRAN_KIND:
    case UNOP_FORTRAN_FLOOR:
    case UNOP_FORTRAN_CEILING:
    case BINOP_FORTRAN_CMPLX:
    case BINOP_FORTRAN_MODULO:
      operator_length_f (exp, (elt + 1), &oplen, &nargs);
      break;

    case OP_F77_UNDETERMINED_ARGLIST:
      return dump_subexp_body_funcall (exp, stream, elt);
    }

  elt += oplen;
  for (i = 0; i < nargs; i += 1)
    elt = dump_subexp (exp, stream, elt);

  return elt;
}

/* Special expression checking for Fortran.  */

static int
operator_check_f (struct expression *exp, int pos,
		  int (*objfile_func) (struct objfile *objfile,
				       void *data),
		  void *data)
{
  const union exp_element *const elts = exp->elts;

  switch (elts[pos].opcode)
    {
    case UNOP_FORTRAN_KIND:
    case UNOP_FORTRAN_FLOOR:
    case UNOP_FORTRAN_CEILING:
    case BINOP_FORTRAN_CMPLX:
    case BINOP_FORTRAN_MODULO:
      /* Any references to objfiles are held in the arguments to this
	 expression, not within the expression itself, so no additional
	 checking is required here, the outer expression iteration code
	 will take care of checking each argument.  */
      break;

    default:
      return operator_check_standard (exp, pos, objfile_func, data);
    }

  return 0;
}

/* Expression processing for Fortran.  */
static const struct exp_descriptor exp_descriptor_f =
{
  print_subexp_f,
  operator_length_f,
  operator_check_f,
  op_name_f,
  dump_subexp_body_f,
  evaluate_subexp_f
};

/* Class representing the Fortran language.  */

class f_language : public language_defn
{
public:
  f_language ()
    : language_defn (language_fortran)
  { /* Nothing.  */ }

  /* See language.h.  */

  const char *name () const override
  { return "fortran"; }

  /* See language.h.  */

  const char *natural_name () const override
  { return "Fortran"; }

  /* See language.h.  */

  const std::vector<const char *> &filename_extensions () const override
  {
    static const std::vector<const char *> extensions = {
      ".f", ".F", ".for", ".FOR", ".ftn", ".FTN", ".fpp", ".FPP",
      ".f90", ".F90", ".f95", ".F95", ".f03", ".F03", ".f08", ".F08"
    };
    return extensions;
  }

  /* See language.h.  */
  void language_arch_info (struct gdbarch *gdbarch,
			   struct language_arch_info *lai) const override
  {
    const struct builtin_f_type *builtin = builtin_f_type (gdbarch);

    lai->string_char_type = builtin->builtin_character;
    lai->primitive_type_vector
      = GDBARCH_OBSTACK_CALLOC (gdbarch, nr_f_primitive_types + 1,
				struct type *);

    lai->primitive_type_vector [f_primitive_type_character]
      = builtin->builtin_character;
    lai->primitive_type_vector [f_primitive_type_logical]
      = builtin->builtin_logical;
    lai->primitive_type_vector [f_primitive_type_logical_s1]
      = builtin->builtin_logical_s1;
    lai->primitive_type_vector [f_primitive_type_logical_s2]
      = builtin->builtin_logical_s2;
    lai->primitive_type_vector [f_primitive_type_logical_s8]
      = builtin->builtin_logical_s8;
    lai->primitive_type_vector [f_primitive_type_real]
      = builtin->builtin_real;
    lai->primitive_type_vector [f_primitive_type_real_s8]
      = builtin->builtin_real_s8;
    lai->primitive_type_vector [f_primitive_type_real_s16]
      = builtin->builtin_real_s16;
    lai->primitive_type_vector [f_primitive_type_complex_s8]
      = builtin->builtin_complex_s8;
    lai->primitive_type_vector [f_primitive_type_complex_s16]
      = builtin->builtin_complex_s16;
    lai->primitive_type_vector [f_primitive_type_void]
      = builtin->builtin_void;

    lai->bool_type_symbol = "logical";
    lai->bool_type_default = builtin->builtin_logical_s2;
  }

  /* See language.h.  */
  unsigned int search_name_hash (const char *name) const override
  {
    return cp_search_name_hash (name);
  }

  /* See language.h.  */

  char *demangle (const char *mangled, int options) const override
  {
      /* We could support demangling here to provide module namespaces
	 also for inferiors with only minimal symbol table (ELF symbols).
	 Just the mangling standard is not standardized across compilers
	 and there is no DW_AT_producer available for inferiors with only
	 the ELF symbols to check the mangling kind.  */
    return nullptr;
  }

  /* See language.h.  */

  void print_type (struct type *type, const char *varstring,
		   struct ui_file *stream, int show, int level,
		   const struct type_print_options *flags) const override
  {
    f_print_type (type, varstring, stream, show, level, flags);
  }

  /* See language.h.  This just returns default set of word break
     characters but with the modules separator `::' removed.  */

  const char *word_break_characters (void) const override
  {
    static char *retval;

    if (!retval)
      {
	char *s;

	retval = xstrdup (language_defn::word_break_characters ());
	s = strchr (retval, ':');
	if (s)
	  {
	    char *last_char = &s[strlen (s) - 1];

	    *s = *last_char;
	    *last_char = 0;
	  }
      }
    return retval;
  }


  /* See language.h.  */

  void collect_symbol_completion_matches (completion_tracker &tracker,
					  complete_symbol_mode mode,
					  symbol_name_match_type name_match_type,
					  const char *text, const char *word,
					  enum type_code code) const override
  {
    /* Consider the modules separator :: as a valid symbol name character
       class.  */
    default_collect_symbol_completion_matches_break_on (tracker, mode,
							name_match_type,
							text, word, ":",
							code);
  }

  /* See language.h.  */

  void value_print_inner
	(struct value *val, struct ui_file *stream, int recurse,
	 const struct value_print_options *options) const override
  {
    return f_value_print_inner (val, stream, recurse, options);
  }

  /* See language.h.  */

  struct block_symbol lookup_symbol_nonlocal
	(const char *name, const struct block *block,
	 const domain_enum domain) const override
  {
    return cp_lookup_symbol_nonlocal (this, name, block, domain);
  }

  /* See language.h.  */

  int parser (struct parser_state *ps) const override
  {
    return f_parse (ps);
  }

  /* See language.h.  */

  void emitchar (int ch, struct type *chtype,
		 struct ui_file *stream, int quoter) const override
  {
    const char *encoding = f_get_encoding (chtype);
    generic_emit_char (ch, chtype, stream, quoter, encoding);
  }

  /* See language.h.  */

  void printchar (int ch, struct type *chtype,
		  struct ui_file *stream) const override
  {
    fputs_filtered ("'", stream);
    LA_EMIT_CHAR (ch, chtype, stream, '\'');
    fputs_filtered ("'", stream);
  }

  /* See language.h.  */

  void printstr (struct ui_file *stream, struct type *elttype,
		 const gdb_byte *string, unsigned int length,
		 const char *encoding, int force_ellipses,
		 const struct value_print_options *options) const override
  {
    const char *type_encoding = f_get_encoding (elttype);

    if (TYPE_LENGTH (elttype) == 4)
      fputs_filtered ("4_", stream);

    if (!encoding || !*encoding)
      encoding = type_encoding;

    generic_printstr (stream, elttype, string, length, encoding,
		      force_ellipses, '\'', 0, options);
  }

  /* See language.h.  */

  void print_typedef (struct type *type, struct symbol *new_symbol,
		      struct ui_file *stream) const override
  {
    f_print_typedef (type, new_symbol, stream);
  }

  /* See language.h.  */

  bool is_string_type_p (struct type *type) const override
  {
    type = check_typedef (type);
    return (type->code () == TYPE_CODE_STRING
	    || (type->code () == TYPE_CODE_ARRAY
		&& TYPE_TARGET_TYPE (type)->code () == TYPE_CODE_CHAR));
  }

  /* See language.h.  */

  const char *struct_too_deep_ellipsis () const override
  { return "(...)"; }

  /* See language.h.  */

  bool c_style_arrays_p () const override
  { return false; }

  /* See language.h.  */

  bool range_checking_on_by_default () const override
  { return true; }

  /* See language.h.  */

  enum case_sensitivity case_sensitivity () const override
  { return case_sensitive_off; }

  /* See language.h.  */

  enum array_ordering array_ordering () const override
  { return array_column_major; }

  /* See language.h.  */

  const struct exp_descriptor *expression_ops () const override
  { return &exp_descriptor_f; }

  /* See language.h.  */

  const struct op_print *opcode_print_table () const override
  { return f_op_print_tab; }

protected:

  /* See language.h.  */

  symbol_name_matcher_ftype *get_symbol_name_matcher_inner
	(const lookup_name_info &lookup_name) const override
  {
    return cp_get_symbol_name_matcher (lookup_name);
  }
};

/* Single instance of the Fortran language class.  */

static f_language f_language_defn;

static void *
build_fortran_types (struct gdbarch *gdbarch)
{
  struct builtin_f_type *builtin_f_type
    = GDBARCH_OBSTACK_ZALLOC (gdbarch, struct builtin_f_type);

  builtin_f_type->builtin_void
    = arch_type (gdbarch, TYPE_CODE_VOID, TARGET_CHAR_BIT, "void");

  builtin_f_type->builtin_character
    = arch_type (gdbarch, TYPE_CODE_CHAR, TARGET_CHAR_BIT, "character");

  builtin_f_type->builtin_logical_s1
    = arch_boolean_type (gdbarch, TARGET_CHAR_BIT, 1, "logical*1");

  builtin_f_type->builtin_integer_s2
    = arch_integer_type (gdbarch, gdbarch_short_bit (gdbarch), 0,
			 "integer*2");

  builtin_f_type->builtin_integer_s8
    = arch_integer_type (gdbarch, gdbarch_long_long_bit (gdbarch), 0,
			 "integer*8");

  builtin_f_type->builtin_logical_s2
    = arch_boolean_type (gdbarch, gdbarch_short_bit (gdbarch), 1,
			 "logical*2");

  builtin_f_type->builtin_logical_s8
    = arch_boolean_type (gdbarch, gdbarch_long_long_bit (gdbarch), 1,
			 "logical*8");

  builtin_f_type->builtin_integer
    = arch_integer_type (gdbarch, gdbarch_int_bit (gdbarch), 0,
			 "integer");

  builtin_f_type->builtin_logical
    = arch_boolean_type (gdbarch, gdbarch_int_bit (gdbarch), 1,
			 "logical*4");

  builtin_f_type->builtin_real
    = arch_float_type (gdbarch, gdbarch_float_bit (gdbarch),
		       "real", gdbarch_float_format (gdbarch));
  builtin_f_type->builtin_real_s8
    = arch_float_type (gdbarch, gdbarch_double_bit (gdbarch),
		       "real*8", gdbarch_double_format (gdbarch));
  auto fmt = gdbarch_floatformat_for_type (gdbarch, "real(kind=16)", 128);
  if (fmt != nullptr)
    builtin_f_type->builtin_real_s16
      = arch_float_type (gdbarch, 128, "real*16", fmt);
  else if (gdbarch_long_double_bit (gdbarch) == 128)
    builtin_f_type->builtin_real_s16
      = arch_float_type (gdbarch, gdbarch_long_double_bit (gdbarch),
			 "real*16", gdbarch_long_double_format (gdbarch));
  else
    builtin_f_type->builtin_real_s16
      = arch_type (gdbarch, TYPE_CODE_ERROR, 128, "real*16");

  builtin_f_type->builtin_complex_s8
    = init_complex_type ("complex*8", builtin_f_type->builtin_real);
  builtin_f_type->builtin_complex_s16
    = init_complex_type ("complex*16", builtin_f_type->builtin_real_s8);

  if (builtin_f_type->builtin_real_s16->code () == TYPE_CODE_ERROR)
    builtin_f_type->builtin_complex_s32
      = arch_type (gdbarch, TYPE_CODE_ERROR, 256, "complex*32");
  else
    builtin_f_type->builtin_complex_s32
      = init_complex_type ("complex*32", builtin_f_type->builtin_real_s16);

  return builtin_f_type;
}

static struct gdbarch_data *f_type_data;

const struct builtin_f_type *
builtin_f_type (struct gdbarch *gdbarch)
{
  return (const struct builtin_f_type *) gdbarch_data (gdbarch, f_type_data);
}

/* Whether GDB should assume arguments to functions without debug are
   artificial or not.  */

static bool fortran_arguments_are_artificial = false;

/* Implement 'show fortran arguments-are-artificial'.  */

static void
show_fortran_arguments_are_artificial (struct ui_file *file, int from_tty,
				       struct cmd_list_element *c,
				       const char *value)
{
  fprintf_filtered (file, _("Assuming arguments to Fortran functions "
			    "without debug are artificial is %s.\n"),
		    value);
}

/* Command-list for the "set/show fortran" prefix command.  */

static struct cmd_list_element *set_fortran_list;
static struct cmd_list_element *show_fortran_list;

void _initialize_f_language ();
void
_initialize_f_language ()
{
  f_type_data = gdbarch_data_register_post_init (build_fortran_types);

  add_basic_prefix_cmd ("fortran", no_class,
                        _("Prefix command for changing Fortran-specific settings."),
                        &set_fortran_list, "set fortran ", 0, &setlist);

  add_show_prefix_cmd ("fortran", no_class,
                       _("Generic command for showing Fortran-specific settings."),
                       &show_fortran_list, "show fortran ", 0, &showlist);

  add_setshow_boolean_cmd ("arguments-are-artificial", class_vars,
                           &fortran_arguments_are_artificial, _("\
Sets whether arguments to functions without debug information are artificial."), _("\
Show whether arguments to functions without debug information are artificial."), _("\
When calling a function in the inferior that does not have debug\n\
information GDB needs to decide if the arguments are artificial or not.\n\
\n\
Artificial arguments are passed by value while non-artificial arguments\n\
are passed by reference.\n\
When this setting is on GDB will assume all arguments are artificial and\n\
pass them by value.  If you need to pass a non-artificial argument then\n\
pass the address of the argument.\n\
\n\
This setting only effects calling functions without debug information.  For\n\
functions with debug information GDB knows which arguments are artificial,\n\
and which are not."),
                           NULL,
                           show_fortran_arguments_are_artificial,
                           &set_fortran_list, &show_fortran_list);
}

/* Ensures that function argument VALUE is in the appropriate form to
   pass to a Fortran function.  Returns a possibly new value that should
   be used instead of VALUE.

   When IS_ARTIFICIAL is true this indicates an artificial argument,
   e.g. hidden string lengths which the GNU Fortran argument passing
   convention specifies as being passed by value.

   When IS_ARTIFICIAL is false, the argument is passed by pointer.  If the
   value is already in target memory then return a value that is a pointer
   to VALUE.  If VALUE is not in memory (e.g. an integer literal), allocate
   space in the target, copy VALUE in, and return a pointer to the in
   memory copy.  */

static struct value *
fortran_argument_convert (struct value *value, bool is_artificial)
{
  if (!is_artificial)
    {
      /* If the value is not in the inferior e.g. registers values,
	 convenience variables and user input.  */
      if (VALUE_LVAL (value) != lval_memory)
	{
	  struct type *type = value_type (value);
	  const int length = TYPE_LENGTH (type);
	  const CORE_ADDR addr
	    = value_as_long (value_allocate_space_in_inferior (length));
	  write_memory (addr, value_contents (value), length);
	  struct value *val
	    = value_from_contents_and_address (type, value_contents (value),
					       addr);
	  return value_addr (val);
	}
      else
	return value_addr (value); /* Program variables, e.g. arrays.  */
    }
    return value;
}

/* See f-lang.h.  */

struct type *
fortran_preserve_arg_pointer (struct value *arg, struct type *type)
{
  if (value_type (arg)->code () == TYPE_CODE_PTR)
    return value_type (arg);
  return type;
}

/* Prepare (and return) an argument value ready for an inferior function
   call to a Fortran function.  EXP and POS are the expressions describing
   the argument to prepare.  ARG_NUM is the argument number being
   prepared, with 0 being the first argument and so on.  FUNC_TYPE is the
   type of the function being called.

   IS_INTERNAL_CALL_P is true if this is a call to a function of type
   TYPE_CODE_INTERNAL_FUNCTION, otherwise this parameter is false.

   NOSIDE has its usual meaning for expression parsing (see eval.c).

   Arguments in Fortran are normally passed by address, we coerce the
   arguments here rather than in value_arg_coerce as otherwise the call to
   malloc (to place the non-lvalue parameters in target memory) is hit by
   this Fortran specific logic.  This results in malloc being called with a
   pointer to an integer followed by an attempt to malloc the arguments to
   malloc in target memory.  Infinite recursion ensues.  */

static value *
fortran_prepare_argument (struct expression *exp, int *pos,
			  int arg_num, bool is_internal_call_p,
			  struct type *func_type, enum noside noside)
{
  if (is_internal_call_p)
    return evaluate_subexp_with_coercion (exp, pos, noside);

  bool is_artificial;
  if (arg_num >= func_type->num_fields ())
    {
      /* We are unable to know if this argument is artificial or not.  The
	 behaviour now depends on 'set fortran arguments-are-artificial'.  */
      if (fortran_arguments_are_artificial)
	{
	  /* If the expression the user is trying to pass here starts by
	     taking the address of a value then they are trying to pass a
	     non-artificial argument, strip away the address-of operator,
	     and allow FORTRAN_ARGUMENT_CONVERT to fix things up.  */
	  if (exp->elts[*pos].opcode == UNOP_ADDR)
	    {
	      ++(*pos);
	      is_artificial = false;
	    }
	  else
	    is_artificial = true;
	}
      else
	is_artificial = false;
    }
  else
    is_artificial = TYPE_FIELD_ARTIFICIAL (func_type, arg_num);

  struct value *arg_val = evaluate_subexp_with_coercion (exp, pos, noside);
  return fortran_argument_convert (arg_val, is_artificial);
}
