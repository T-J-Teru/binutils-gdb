/* YACC parser for Fortran expressions, for GDB.
   Copyright (C) 1986-2013 Free Software Foundation, Inc.

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

/* This was blantantly ripped off the C expression parser, please 
   be aware of that as you look at its basic structure -FMB */ 

/* Parse a F77 expression from text in a string,
   and return the result as a  struct expression  pointer.
   That structure contains arithmetic operations in reverse polish,
   with constants represented by operations that are followed by special data.
   See expression.h for the details of the format.
   What is important here is that it can be built up sequentially
   during the process of parsing; the lower levels of the tree always
   come first in the result.

   Note that malloc's and realloc's in this file are transformed to
   xmalloc and xrealloc respectively by the same sed command in the
   makefile that remaps any other malloc/realloc inserted by the parser
   generator.  Doing this with #defines and trying to control the interaction
   with include files (<malloc.h> and <stdlib.h> for example) just became
   too messy, particularly when such includes can be inserted at random
   times by the parser generator.  */
   
%{

#include "defs.h"
#include "gdb_string.h"
#include "expression.h"
#include "value.h"
#include "parser-defs.h"
#include "language.h"
#include "f-lang.h"
#include "f-module.h"
#include "bfd.h" /* Required by objfiles.h.  */
#include "symfile.h" /* Required by objfiles.h.  */
#include "objfiles.h" /* For have_full_symbols and have_partial_symbols */
#include "block.h"
#include <ctype.h>

#define parse_type builtin_type (parse_gdbarch)
#define parse_f_type builtin_f_type (parse_gdbarch)

/* Remap normal yacc parser interface names (yyparse, yylex, yyerror, etc),
   as well as gratuitiously global symbol names, so we can have multiple
   yacc generated parsers in gdb.  Note that these are only the variables
   produced by yacc.  If other parser generators (bison, byacc, etc) produce
   additional global names that conflict at link time, then those parser
   generators need to be fixed instead of adding those names to this list.  */

#define	yymaxdepth f_maxdepth
#define	yyparse	f_parse
#define	yylex	f_lex
#define	yyerror	f_error
#define	yylval	f_lval
#define	yychar	f_char
#define	yydebug	f_debug
#define	yypact	f_pact	
#define	yyr1	f_r1			
#define	yyr2	f_r2			
#define	yydef	f_def		
#define	yychk	f_chk		
#define	yypgo	f_pgo		
#define	yyact	f_act		
#define	yyexca	f_exca
#define yyerrflag f_errflag
#define yynerrs	f_nerrs
#define	yyps	f_ps
#define	yypv	f_pv
#define	yys	f_s
#define	yy_yys	f_yys
#define	yystate	f_state
#define	yytmp	f_tmp
#define	yyv	f_v
#define	yy_yyv	f_yyv
#define	yyval	f_val
#define	yylloc	f_lloc
#define yyreds	f_reds		/* With YYDEBUG defined */
#define yytoks	f_toks		/* With YYDEBUG defined */
#define yyname	f_name		/* With YYDEBUG defined */
#define yyrule	f_rule		/* With YYDEBUG defined */
#define yylhs	f_yylhs
#define yylen	f_yylen
#define yydefred f_yydefred
#define yydgoto	f_yydgoto
#define yysindex f_yysindex
#define yyrindex f_yyrindex
#define yygindex f_yygindex
#define yytable	 f_yytable
#define yycheck	 f_yycheck
#define yyss	f_yyss
#define yysslim	f_yysslim
#define yyssp	f_yyssp
#define yystacksize f_yystacksize
#define yyvs	f_yyvs
#define yyvsp	f_yyvsp

#ifndef YYDEBUG
#define	YYDEBUG	1		/* Default to yydebug support */
#endif

#define YYFPRINTF parser_fprintf

int yyparse (void);

static int yylex (void);

void yyerror (char *);

static void growbuf_by_size (int);

static int match_string_literal (void);

static struct type *follow_f_types (struct type *follow_type);

%}

/* Although the yacc "value" of an expression is not used,
   since the result is stored in the structure being created,
   other node types do have values.  */

%union
  {
    LONGEST lval;
    struct {
      LONGEST val;
      struct type *type;
    } typed_val;
    DOUBLEST dval;
    struct symbol *sym;
    struct type *tval;
    struct stoken sval;
    struct ttype tsym;
    struct symtoken ssym;
    int voidval;
    struct block *bval;
    enum exp_opcode opcode;
    struct internalvar *ivar;

    struct type **tvec;
    int *ivec;
  }

%{
/* YYSTYPE gets defined by %union */
static int parse_number (char *, int, int, YYSTYPE *);
%}

%type <voidval> exp  type_exp start variable 
%type <tval> type typebase
/* %type <bval> block */

/* Fancy type parsing.  */
%type <tval> ptype

%type <lval> signed_int

%token <typed_val> INT
%token <dval> FLOAT

/* Both NAME and TYPENAME tokens represent symbols in the input,
   and both convey their data as strings.
   But a TYPENAME is a string that happens to be defined as a typedef
   or builtin type name (such as int or char)
   and a NAME is any other symbol.
   Contexts where this distinction is not important can use the
   nonterminal "name", which matches either NAME or TYPENAME.  */

%token <sval> STRING_LITERAL
%token <lval> BOOLEAN_LITERAL
%token <ssym> NAME 
%token <tsym> TYPENAME
%type <sval> name
%type <ssym> name_not_typename

/* A NAME_OR_INT is a symbol which is not known in the symbol table,
   but which would parse as a valid number in the current input radix.
   E.g. "c" when input_radix==16.  Depending on the parse, it will be
   turned into a name or into a number.  */

%token <ssym> NAME_OR_INT 

%token POINTER SIZEOF COLONCOLON KIND
%token ERROR

/* Special type cases, put in to allow the parser to distinguish different
   legal basetypes.  */
%token INT_KEYWORD INT_S2_KEYWORD INT_S8_KEYWORD LOGICAL_S1_KEYWORD LOGICAL_S2_KEYWORD
%token LOGICAL_S8_KEYWORD
%token LOGICAL_KEYWORD REAL_KEYWORD REAL_S8_KEYWORD REAL_S16_KEYWORD 
%token COMPLEX_S8_KEYWORD COMPLEX_S16_KEYWORD COMPLEX_S32_KEYWORD 
%token BOOL_AND BOOL_OR BOOL_NOT BIN_MOD
%token <lval> CHARACTER 

%token <voidval> VARIABLE

%token <opcode> ASSIGN_MODIFY
%token <opcode> UNOP_INTRINSIC
%token <opcode> BINOP_INTRINSIC

%left ','
%left ABOVE_COMMA
%right '=' ASSIGN_MODIFY
%right '?'
%left BOOL_OR
%right BOOL_NOT
%left BOOL_AND
%left '|'
%left '^'
%left '&'
%left EQUAL NOTEQUAL
%left LESSTHAN GREATERTHAN LEQ GEQ
%left LSH RSH
%left '@'
%left '+' '-'
%left '*' '/'
%right STARSTAR
%right '%'
%right UNARY 
%right '('
%right ARROW
%left COLONCOLON
%left BELOW_SIZE
%left SIZE


%%

start   :	exp
	|	type_exp
	;

type_exp:	type
			{ write_exp_elt_opcode(OP_TYPE);
			  write_exp_elt_type($1);
			  write_exp_elt_opcode(OP_TYPE); }
	;

exp     :       '(' exp ')'
        		{ }
        ;

/* Expressions, not including the comma operator.  */
exp	:	'*' exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_IND); }
	;

exp	:	'&' exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_ADDR); }
	;

exp	:	'-' exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_NEG); }
	;

exp	:	BOOL_NOT exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_LOGICAL_NOT); }
	;

exp	:	'~' exp    %prec UNARY
			{ write_exp_elt_opcode (UNOP_COMPLEMENT); }
	;

exp	:	SIZEOF exp       %prec UNARY
			{ write_exp_elt_opcode (UNOP_SIZEOF); }
	;

/* No more explicit array operators, we treat everything in F77 as 
   a function call.  The disambiguation as to whether we are 
   doing a subscript operation or a function call is done 
   later in eval.c.  */

exp	:	exp '(' 
			{ start_arglist (); }
		arglist ')'	
			{ write_exp_elt_opcode (OP_F77_UNDETERMINED_ARGLIST);
			  write_exp_elt_longcst ((LONGEST) end_arglist ());
			  write_exp_elt_opcode (OP_F77_UNDETERMINED_ARGLIST); }
	;

exp     :       UNOP_INTRINSIC '(' exp ')'
                        { write_exp_elt_opcode ($1); }
        ;

exp     :       BINOP_INTRINSIC '(' exp ',' exp ')'
                        { write_exp_elt_opcode ($1); }

arglist	:
	;

arglist	:	exp
			{ arglist_len = 1; }
	;

arglist :	subrange
			{ arglist_len = 1; }
	;
   
arglist	:	arglist ',' exp   %prec ABOVE_COMMA
			{ arglist_len++; }
	;

arglist	:	arglist ',' subrange   %prec ABOVE_COMMA
			{ arglist_len++; }
	;

/* There are four sorts of subrange types in F90.  */

subrange:	exp ':' exp	%prec ABOVE_COMMA
			{ write_exp_elt_opcode (OP_F90_RANGE); 
			  write_exp_elt_longcst (NONE_BOUND_DEFAULT);
			  write_exp_elt_opcode (OP_F90_RANGE); }
	;

subrange:	exp colon_star	%prec ABOVE_COMMA
			{ write_exp_elt_opcode (OP_F90_RANGE);
			  write_exp_elt_longcst (HIGH_BOUND_DEFAULT);
			  write_exp_elt_opcode (OP_F90_RANGE); }
	;

subrange:	star_colon exp	%prec ABOVE_COMMA
			{ write_exp_elt_opcode (OP_F90_RANGE);
			  write_exp_elt_longcst (LOW_BOUND_DEFAULT);
			  write_exp_elt_opcode (OP_F90_RANGE); }
	;

subrange:	star_colon_star	%prec ABOVE_COMMA
			{ write_exp_elt_opcode (OP_F90_RANGE);
			  write_exp_elt_longcst (BOTH_BOUND_DEFAULT);
			  write_exp_elt_opcode (OP_F90_RANGE); }
	;

complexnum:     exp ',' exp 
                	{ }                          
        ;

exp	:	'(' complexnum ')'
                	{ write_exp_elt_opcode(OP_COMPLEX);
			  write_exp_elt_type (parse_f_type->builtin_complex_s16);
                	  write_exp_elt_opcode(OP_COMPLEX); }
	;

exp	:	'(' type ')' exp  %prec UNARY
			{ write_exp_elt_opcode (UNOP_CAST);
			  write_exp_elt_type ($2);
			  write_exp_elt_opcode (UNOP_CAST); }
	;

exp     :       exp '%' name
                        { write_exp_elt_opcode (STRUCTOP_STRUCT);
                          write_exp_string ($3);
                          write_exp_elt_opcode (STRUCTOP_STRUCT); }
        ;

/* Binary operators in order of decreasing precedence.  */

exp	:	exp '@' exp
			{ write_exp_elt_opcode (BINOP_REPEAT); }
	;

exp	:	exp STARSTAR exp
			{ write_exp_elt_opcode (BINOP_EXP); }
	;

exp	:	exp '*' exp
			{ write_exp_elt_opcode (BINOP_MUL); }
	;

exp	:	exp '/' exp
			{ write_exp_elt_opcode (BINOP_DIV); }
	;

exp	:	exp '+' exp
			{ write_exp_elt_opcode (BINOP_ADD); }
	;

exp	:	exp '-' exp
			{ write_exp_elt_opcode (BINOP_SUB); }
	;

exp	:	exp LSH exp
			{ write_exp_elt_opcode (BINOP_LSH); }
	;

exp	:	exp RSH exp
			{ write_exp_elt_opcode (BINOP_RSH); }
	;

exp	:	exp EQUAL exp
			{ write_exp_elt_opcode (BINOP_EQUAL); }
	;

exp	:	exp NOTEQUAL exp
			{ write_exp_elt_opcode (BINOP_NOTEQUAL); }
	;

exp	:	exp LEQ exp
			{ write_exp_elt_opcode (BINOP_LEQ); }
	;

exp	:	exp GEQ exp
			{ write_exp_elt_opcode (BINOP_GEQ); }
	;

exp	:	exp LESSTHAN exp
			{ write_exp_elt_opcode (BINOP_LESS); }
	;

exp	:	exp GREATERTHAN exp
			{ write_exp_elt_opcode (BINOP_GTR); }
	;

exp	:	exp '>' exp
			{ write_exp_elt_opcode (BINOP_GTR); }
	;

exp	:	exp '<' exp
			{ write_exp_elt_opcode (BINOP_LESS); }
	;

exp	:	exp '&' exp
			{ write_exp_elt_opcode (BINOP_BITWISE_AND); }
	;

exp	:	exp '^' exp
			{ write_exp_elt_opcode (BINOP_BITWISE_XOR); }
	;

exp	:	exp '|' exp
			{ write_exp_elt_opcode (BINOP_BITWISE_IOR); }
	;

exp     :       exp BOOL_AND exp
			{ write_exp_elt_opcode (BINOP_LOGICAL_AND); }
	;

exp	:	exp BOOL_OR exp
			{ write_exp_elt_opcode (BINOP_LOGICAL_OR); }
	;

exp	:	exp BIN_MOD exp
			{ write_exp_elt_opcode (BINOP_REM); }
	;

exp	:	exp '=' exp
			{ write_exp_elt_opcode (BINOP_ASSIGN); }
	;

exp	:	exp ASSIGN_MODIFY exp
			{ write_exp_elt_opcode (BINOP_ASSIGN_MODIFY);
			  write_exp_elt_opcode ($2);
			  write_exp_elt_opcode (BINOP_ASSIGN_MODIFY); }
	;

exp	:	INT
			{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type ($1.type);
			  write_exp_elt_longcst ((LONGEST)($1.val));
			  write_exp_elt_opcode (OP_LONG); }
	;

exp	:	NAME_OR_INT
			{ YYSTYPE val;
			  parse_number ($1.stoken.ptr, $1.stoken.length, 0, &val);
			  write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (val.typed_val.type);
			  write_exp_elt_longcst ((LONGEST)val.typed_val.val);
			  write_exp_elt_opcode (OP_LONG); }
	;

exp	:	FLOAT
			{ write_exp_elt_opcode (OP_DOUBLE);
			  write_exp_elt_type (parse_f_type->builtin_real_s8);
			  write_exp_elt_dblcst ($1);
			  write_exp_elt_opcode (OP_DOUBLE); }
	;

exp	:	variable
	;

exp	:	VARIABLE
	;

exp	:	SIZEOF '(' type ')'	%prec UNARY
			{ write_exp_elt_opcode (OP_LONG);
			  write_exp_elt_type (parse_f_type->builtin_integer);
			  CHECK_TYPEDEF ($3);
			  write_exp_elt_longcst ((LONGEST) TYPE_LENGTH ($3));
			  write_exp_elt_opcode (OP_LONG); }
	;

exp     :       BOOLEAN_LITERAL
			{ write_exp_elt_opcode (OP_BOOL);
			  write_exp_elt_longcst ((LONGEST) $1);
			  write_exp_elt_opcode (OP_BOOL);
			}
        ;

exp	:	STRING_LITERAL
			{
			  write_exp_elt_opcode (OP_STRING);
			  write_exp_string ($1);
			  write_exp_elt_opcode (OP_STRING);
			}
	;

exp	:	exp ARROW name
			{ write_exp_elt_opcode (STRUCTOP_PTR);
			  write_exp_string ($3);
			  write_exp_elt_opcode (STRUCTOP_PTR); }
	;

exp	:	exp '.' name
			{ write_exp_elt_opcode (STRUCTOP_STRUCT);
			  write_exp_string ($3);
			  write_exp_elt_opcode (STRUCTOP_STRUCT); }
	;

variable:	name_not_typename
			{ struct symbol *sym = $1.sym;

			  if (sym)
			    {
			      if (symbol_read_needs_frame (sym))
				{
				  if (innermost_block == 0
				      || contained_in (block_found, 
						       innermost_block))
				    innermost_block = block_found;
				}
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      /* We want to use the selected frame, not
				 another more inner frame which happens to
				 be in the same block.  */
			      write_exp_elt_block (NULL);
			      write_exp_elt_sym (sym);
			      write_exp_elt_opcode (OP_VAR_VALUE);
			      break;
			    }
			  else
			    {
			      struct minimal_symbol *msymbol;
			      char *arg = copy_name ($1.stoken);

			      msymbol =
				lookup_minimal_symbol (arg, NULL, NULL);
			      if (msymbol != NULL)
				write_exp_msymbol (msymbol);
			      else if (!have_full_symbols () && !have_partial_symbols ())
				error (_("No symbol table is loaded.  Use the \"file\" command."));
			      else
				error (_("No symbol \"%s\" in current context."),
				       copy_name ($1.stoken));
			    }
			}
	;

type    :       ptype
        ;

ptype	:	typebase
	|	typebase ',' POINTER
			{ $$ = lookup_pointer_type ($1); }
	|	typebase array_mod
			{ $$ = follow_f_types ($1); }
	|	typebase ',' POINTER array_mod
			{ $$ = lookup_pointer_type (follow_f_types ($1)); }
	;

array_mod:	'(' range ')'
	;

range:		subrange2 ',' subrange2
	|	subrange2
	;

subrange2:	signed_int ':' signed_int
			{
			  push_type_int ($1);
			  push_type_int ($3);
			  push_type (tp_array); }
	;

subrange2:	signed_int colon_star
			{
			  push_type_int ($1);
			  push_type_int ($1 - 1);
			  push_type (tp_array); }
	;

subrange2:	star_colon signed_int
			{ push_type_int (1);
			  push_type_int ($2);
			  push_type (tp_array); }
	;

subrange2:	star_colon_star
			{ push_type_int (1);
			  push_type_int (0);
			  push_type (tp_array); }
	;

subrange2:	signed_int
			{ push_type_int (1);
			  push_type_int ($1);
			  push_type (tp_array); }
	;

star_colon:	':'
	|	'*' ':'
	;

colon_star:	':'
	|	':' '*'
	;

star_colon_star:	':'
	|		'*'
	|		':' '*'
	|		'*' '|'
	|		'*' ':' '*'
	;

signed_int:	INT
			{ $$ = $1.val; }
	|	'-' INT
			{ $$ = -$2.val; }
	;

typebase
	:	TYPENAME
			{ $$ = $1.type; }
	|	INT_KEYWORD	%prec BELOW_SIZE
			{ $$ = parse_f_type->builtin_integer; }
	|	INT_KEYWORD '*' INT	%prec SIZE
			{ if ($3.val == 2)
			  	$$ = parse_f_type->builtin_integer_s2;
			  else if ($3.val == 4)
			  	$$ = parse_f_type->builtin_integer;
			  else if ($3.val == 8)
				$$ = parse_f_type->builtin_integer_s8; }
	|	INT_KEYWORD KIND '=' INT ')'	%prec SIZE
			{ if ($4.val == 2)
			  	$$ = parse_f_type->builtin_integer_s2;
			  else if ($4.val == 4)
			  	$$ = parse_f_type->builtin_integer;
			  else if ($4.val == 8)
				$$ = parse_f_type->builtin_integer_s8; }
	|	INT_S2_KEYWORD 
			{ $$ = parse_f_type->builtin_integer_s2; }
	|	INT_S8_KEYWORD 
			{ $$ = parse_f_type->builtin_integer_s8; }
	|	CHARACTER 
			{ $$ = parse_f_type->builtin_character; }
	|	LOGICAL_KEYWORD	%prec BELOW_SIZE
			{ $$ = parse_f_type->builtin_logical;} 
	|	LOGICAL_KEYWORD '*' INT	%prec SIZE
			{ if ($3.val == 1)
				$$ = parse_f_type->builtin_logical_s1;
			  else if ($3.val == 2)
				$$ = parse_f_type->builtin_logical_s2;
			  else if ($3.val == 4)
				$$ = parse_f_type->builtin_logical;
			  else if ($3.val == 8)
				$$ = parse_f_type->builtin_logical_s8; }
	|	LOGICAL_KEYWORD KIND '=' INT ')'	%prec SIZE
			{ if ($4.val == 1)
				$$ = parse_f_type->builtin_logical_s1;
			  else if ($4.val == 2)
				$$ = parse_f_type->builtin_logical_s2;
			  else if ($4.val == 4)
				$$ = parse_f_type->builtin_logical;
			  else if ($4.val == 8)
				$$ = parse_f_type->builtin_logical_s8; }
	|	LOGICAL_S8_KEYWORD
			{ $$ = parse_f_type->builtin_logical_s8;}
	|	LOGICAL_S2_KEYWORD
			{ $$ = parse_f_type->builtin_logical_s2;}
	|	LOGICAL_S1_KEYWORD 
			{ $$ = parse_f_type->builtin_logical_s1;}
	|	REAL_KEYWORD	 %prec BELOW_SIZE
			{ $$ = parse_f_type->builtin_real;}
	|	REAL_KEYWORD '*' INT	%prec SIZE
			{ if ($3.val == 4)
				$$ = parse_f_type->builtin_real;
			  else if ($3.val == 8)
				$$ = parse_f_type->builtin_real_s8;
			  else if ($3.val == 16)
				$$ = parse_f_type->builtin_real_s16; }
	|	REAL_KEYWORD KIND '=' INT ')'	%prec SIZE
			{ if ($4.val == 4)
				$$ = parse_f_type->builtin_real;
			  else if ($4.val == 8)
				$$ = parse_f_type->builtin_real_s8;
			  else if ($4.val == 16)
				$$ = parse_f_type->builtin_real_s16; }
	|	REAL_S8_KEYWORD
			{ $$ = parse_f_type->builtin_real_s8;}
	|	REAL_S16_KEYWORD
			{ $$ = parse_f_type->builtin_real_s16; }
	|	COMPLEX_S8_KEYWORD '*' INT	%prec SIZE
			{ if ($3.val == 8)
				$$ = parse_f_type->builtin_complex_s8;
			  else if ($3.val == 16)
				$$ = parse_f_type->builtin_complex_s16;
			  else if ($3.val == 32)
				$$ = parse_f_type->builtin_complex_s32; }
	|	COMPLEX_S8_KEYWORD KIND '=' INT ')'	%prec SIZE
			{ if ($4.val == 8)
				$$ = parse_f_type->builtin_complex_s8;
			  else if ($4.val == 16)
				$$ = parse_f_type->builtin_complex_s16;
			  else if ($4.val == 32)
				$$ = parse_f_type->builtin_complex_s32; }
	|	COMPLEX_S8_KEYWORD	 %prec BELOW_SIZE
			{ $$ = parse_f_type->builtin_complex_s8;}
	|	COMPLEX_S16_KEYWORD 
			{ $$ = parse_f_type->builtin_complex_s16;}
	|	COMPLEX_S32_KEYWORD 
			{ $$ = parse_f_type->builtin_complex_s32; }
	;

name	:	NAME
			{ $$ = $1.stoken; }
	|	TYPENAME
			{ $$ = $1.stoken; }
	|	NAME_OR_INT
			{ $$ = $1.stoken; }
	;

name_not_typename :	NAME
/* These would be useful if name_not_typename was useful, but it is just
   a fake for "variable", so these cause reduce/reduce conflicts because
   the parser can't tell whether NAME_OR_INT is a name_not_typename (=variable,
   =exp) or just an exp.  If name_not_typename was ever used in an lvalue
   context where only a name could occur, this might be useful.
  	|	NAME_OR_INT
   */
	;

%%

/* Take care of parsing a number (anything that starts with a digit).
   Set yylval and return the token type; update lexptr.
   LEN is the number of characters in it.  */

/*** Needs some error checking for the float case ***/

static int
parse_number (char *p, int len, int parsed_float, YYSTYPE *putithere)
{
  LONGEST n = 0;
  LONGEST prevn = 0;
  int c;
  int base = input_radix;
  int unsigned_p = 0;
  int long_p = 0;
  ULONGEST high_bit;
  struct type *signed_type;
  struct type *unsigned_type;

  if (parsed_float)
    {
      /* It's a float since it contains a point or an exponent.  */
      /* [dD] is not understood as an exponent by atof, change it to 'e'.  */
      char *tmp, *tmp2;

      tmp = xstrdup (p);
      for (tmp2 = tmp; *tmp2; ++tmp2)
	if (*tmp2 == 'd' || *tmp2 == 'D')
	  *tmp2 = 'e';
      putithere->dval = atof (tmp);
      free (tmp);
      return FLOAT;
    }

  /* Handle base-switching prefixes 0x, 0t, 0d, 0 */
  if (p[0] == '0')
    switch (p[1])
      {
      case 'x':
      case 'X':
	if (len >= 3)
	  {
	    p += 2;
	    base = 16;
	    len -= 2;
	  }
	break;
	
      case 't':
      case 'T':
      case 'd':
      case 'D':
	if (len >= 3)
	  {
	    p += 2;
	    base = 10;
	    len -= 2;
	  }
	break;
	
      default:
	base = 8;
	break;
      }
  
  while (len-- > 0)
    {
      c = *p++;
      if (isupper (c))
	c = tolower (c);
      if (len == 0 && c == 'l')
	long_p = 1;
      else if (len == 0 && c == 'u')
	unsigned_p = 1;
      else
	{
	  int i;
	  if (c >= '0' && c <= '9')
	    i = c - '0';
	  else if (c >= 'a' && c <= 'f')
	    i = c - 'a' + 10;
	  else
	    return ERROR;	/* Char not a digit */
	  if (i >= base)
	    return ERROR;		/* Invalid digit in this base */
	  n *= base;
	  n += i;
	}
      /* Portably test for overflow (only works for nonzero values, so make
	 a second check for zero).  */
      if ((prevn >= n) && n != 0)
	unsigned_p=1;		/* Try something unsigned */
      /* If range checking enabled, portably test for unsigned overflow.  */
      if (RANGE_CHECK && n != 0)
	{
	  if ((unsigned_p && (unsigned)prevn >= (unsigned)n))
	    range_error (_("Overflow on numeric constant."));
	}
      prevn = n;
    }
  
  /* If the number is too big to be an int, or it's got an l suffix
     then it's a long.  Work out if this has to be a long by
     shifting right and seeing if anything remains, and the
     target int size is different to the target long size.
     
     In the expression below, we could have tested
     (n >> gdbarch_int_bit (parse_gdbarch))
     to see if it was zero,
     but too many compilers warn about that, when ints and longs
     are the same size.  So we shift it twice, with fewer bits
     each time, for the same result.  */
  
  if ((gdbarch_int_bit (parse_gdbarch) != gdbarch_long_bit (parse_gdbarch)
       && ((n >> 2)
	   >> (gdbarch_int_bit (parse_gdbarch)-2))) /* Avoid shift warning */
      || long_p)
    {
      high_bit = ((ULONGEST)1) << (gdbarch_long_bit (parse_gdbarch)-1);
      unsigned_type = parse_type->builtin_unsigned_long;
      signed_type = parse_type->builtin_long;
    }
  else 
    {
      high_bit = ((ULONGEST)1) << (gdbarch_int_bit (parse_gdbarch)-1);
      unsigned_type = parse_type->builtin_unsigned_int;
      signed_type = parse_type->builtin_int;
    }    
  
  putithere->typed_val.val = n;
  
  /* If the high bit of the worked out type is set then this number
     has to be unsigned.  */
  
  if (unsigned_p || (n & high_bit)) 
    putithere->typed_val.type = unsigned_type;
  else 
    putithere->typed_val.type = signed_type;
  
  return INT;
}

struct token
{
  char *operator;
  int token;
  enum exp_opcode opcode;
};

static const struct token dot_ops[] =
{
  { ".and.", BOOL_AND, BINOP_END },
  { ".AND.", BOOL_AND, BINOP_END },
  { ".or.", BOOL_OR, BINOP_END },
  { ".OR.", BOOL_OR, BINOP_END },
  { ".mod.", BIN_MOD, BINOP_END },
  { ".MOD.", BIN_MOD, BINOP_END },
  { "==", EQUAL, BINOP_END },
  { ".not.", BOOL_NOT, BINOP_END },
  { ".NOT.", BOOL_NOT, BINOP_END },
  { ".eq.", EQUAL, BINOP_END },
  { ".EQ.", EQUAL, BINOP_END },
  { ".eqv.", EQUAL, BINOP_END },
  { ".NEQV.", NOTEQUAL, BINOP_END },
  { ".neqv.", NOTEQUAL, BINOP_END },
  { ".EQV.", EQUAL, BINOP_END },
  { "!=", NOTEQUAL, BINOP_END },
  { ".ne.", NOTEQUAL, BINOP_END },
  { ".NE.", NOTEQUAL, BINOP_END },
  { ".le.", LEQ, BINOP_END },
  { ".LE.", LEQ, BINOP_END },
  { "<=", LEQ, BINOP_END },
  { ".ge.", GEQ, BINOP_END },
  { ".GE.", GEQ, BINOP_END },
  { ">=", GEQ, BINOP_END },
  { ".gt.", GREATERTHAN, BINOP_END },
  { ".GT.", GREATERTHAN, BINOP_END },
  { ".lt.", LESSTHAN, BINOP_END },
  { ".LT.", LESSTHAN, BINOP_END },
  { NULL, 0, 0 }
};

struct f77_boolean_val 
{
  char *name;
  int value;
}; 

static const struct f77_boolean_val boolean_values[]  = 
{
  { ".true.", 1 },
  { ".TRUE.", 1 },
  { ".false.", 0 },
  { ".FALSE.", 0 },
  { NULL, 0 }
};

static const struct token f77_keywords[] = 
{
  { "complex_16", COMPLEX_S16_KEYWORD, BINOP_END },
  { "complex_32", COMPLEX_S32_KEYWORD, BINOP_END },
  { "character", CHARACTER, BINOP_END },
  { "integer_2", INT_S2_KEYWORD, BINOP_END },
  { "logical_1", LOGICAL_S1_KEYWORD, BINOP_END },
  { "logical_2", LOGICAL_S2_KEYWORD, BINOP_END },
  { "logical_8", LOGICAL_S8_KEYWORD, BINOP_END },
  { "complex_8", COMPLEX_S8_KEYWORD, BINOP_END },
  { "integer", INT_KEYWORD, BINOP_END },
  { "integer_8", INT_S8_KEYWORD, BINOP_END },
  { "logical", LOGICAL_KEYWORD, BINOP_END },
  { "real_16", REAL_S16_KEYWORD, BINOP_END },
  { "complex", COMPLEX_S8_KEYWORD, BINOP_END },
  { "sizeof", SIZEOF, BINOP_END },
  { "real_8", REAL_S8_KEYWORD, BINOP_END },
  { "real", REAL_KEYWORD, BINOP_END },
  { "pointer", POINTER, BINOP_END },
  { NULL, 0, 0 }
}; 

static const struct token intrinsics[] =
  {
    {"ABS", UNOP_INTRINSIC, UNOP_FABS},
    {"AIMAG", UNOP_INTRINSIC, UNOP_CIMAG},
    {"CMPLX", BINOP_INTRINSIC, BINOP_CMPLX},
    {"REALPART", UNOP_INTRINSIC, UNOP_CREAL},
    {"ISINF", UNOP_INTRINSIC, UNOP_IEEE_IS_INF},
    {"IEEE_IS_INF", UNOP_INTRINSIC, UNOP_IEEE_IS_INF},
    {"ISFINITE", UNOP_INTRINSIC, UNOP_IEEE_IS_FINITE},
    {"IEEE_IS_FINITE", UNOP_INTRINSIC, UNOP_IEEE_IS_FINITE},
    {"ISNAN", UNOP_INTRINSIC, UNOP_IEEE_IS_NAN},
    {"IEEE_IS_NAN", UNOP_INTRINSIC, UNOP_IEEE_IS_NAN},
    {"ISNORMAL", UNOP_INTRINSIC, UNOP_IEEE_IS_NORMAL},
    {"IEEE_IS_NORMAL", UNOP_INTRINSIC, UNOP_IEEE_IS_NORMAL},
    {"CEILING", UNOP_INTRINSIC, UNOP_CEIL},
    {"FLOOR", UNOP_INTRINSIC, UNOP_FLOOR},
    {"MOD", BINOP_INTRINSIC, BINOP_FMOD},
    {"MODULO", BINOP_INTRINSIC, BINOP_MODULO},
  };

/* Implementation of a dynamically expandable buffer for processing input
   characters acquired through lexptr and building a value to return in
   yylval.  Ripped off from ch-exp.y */ 

static char *tempbuf;		/* Current buffer contents */
static int tempbufsize;		/* Size of allocated buffer */
static int tempbufindex;	/* Current index into buffer */

#define GROWBY_MIN_SIZE 64	/* Minimum amount to grow buffer by */

#define CHECKBUF(size) \
  do { \
    if (tempbufindex + (size) >= tempbufsize) \
      { \
	growbuf_by_size (size); \
      } \
  } while (0);


/* Grow the static temp buffer if necessary, including allocating the
   first one on demand.  */

static void
growbuf_by_size (int count)
{
  int growby;

  growby = max (count, GROWBY_MIN_SIZE);
  tempbufsize += growby;
  if (tempbuf == NULL)
    tempbuf = (char *) malloc (tempbufsize);
  else
    tempbuf = (char *) realloc (tempbuf, tempbufsize);
}

/* Blatantly ripped off from ch-exp.y. This routine recognizes F77 
   string-literals.
   
   Recognize a string literal.  A string literal is a nonzero sequence
   of characters enclosed in matching single quotes, except that
   a single character inside single quotes is a character literal, which
   we reject as a string literal.  To embed the terminator character inside
   a string, it is simply doubled (I.E. 'this''is''one''string') */

static int
match_string_literal (void)
{
  char *tokptr = lexptr;

  for (tempbufindex = 0, tokptr++; *tokptr != '\0'; tokptr++)
    {
      CHECKBUF (1);
      if (*tokptr == *lexptr)
	{
	  if (*(tokptr + 1) == *lexptr)
	    tokptr++;
	  else
	    break;
	}
      tempbuf[tempbufindex++] = *tokptr;
    }
  if (*tokptr == '\0'					/* no terminator */
      || tempbufindex == 0)				/* no string */
    return 0;
  else
    {
      tempbuf[tempbufindex] = '\0';
      yylval.sval.ptr = tempbuf;
      yylval.sval.length = tempbufindex;
      lexptr = ++tokptr;
      return STRING_LITERAL;
    }
}

static struct type *
follow_f_types (struct type *follow_type)
{
  /* This is where the interesting stuff happens.  */
  int done = 0;
  int lower_bound, upper_bound;
  int *bounds, index;
  enum type_pieces type;
  struct type *range_type;

  while (!done)
    switch (pop_type ())
      {
      case tp_end:
		done = 1;
		break;
      case tp_pointer:
		follow_type = lookup_pointer_type (follow_type);
		break;
      case tp_array:
		bounds = xmalloc (80 * sizeof (int));
		index = 0;
		do {
		  upper_bound = pop_type_int ();
		  lower_bound = pop_type_int ();
		  bounds[index++] = upper_bound;
		  bounds[index++] = lower_bound;
		} while ((type = pop_type()) == tp_array);
		push_type (type);
		while (index > 0)
		  {
			lower_bound = bounds[--index];
			upper_bound = bounds[--index];
			range_type =
				create_range_type ((struct type *) NULL,
				  parse_f_type->builtin_integer, lower_bound,
				  upper_bound);
			follow_type =
				create_array_type ((struct type *) NULL,
				  follow_type, range_type);
		  }
		xfree (bounds);
		break;
	  }
  return follow_type;
}

/* Read one token, getting characters through lexptr.  */

static int
yylex (void)
{
  int c;
  int namelen;
  unsigned int i,token;
  char *tokstart;
  
 retry:
 
  prev_lexptr = lexptr;
 
  tokstart = lexptr;
  
  /* First of all, let us make sure we are not dealing with the 
     special tokens .true. and .false. which evaluate to 1 and 0.  */
  
  if (*lexptr == '.')
    { 
      for (i = 0; boolean_values[i].name != NULL; i++)
	{
	  if (strncasecmp (tokstart, boolean_values[i].name,
		       strlen (boolean_values[i].name)) == 0)
	    {
	      lexptr += strlen (boolean_values[i].name); 
	      yylval.lval = boolean_values[i].value; 
	      return BOOLEAN_LITERAL;
	    }
	}
    }
  
  /* See if it is a special .foo. operator.  */
  
  for (i = 0; dot_ops[i].operator != NULL; i++)
    if (strncasecmp (tokstart, dot_ops[i].operator,
		 strlen (dot_ops[i].operator)) == 0)
      {
	lexptr += strlen (dot_ops[i].operator);
	yylval.opcode = dot_ops[i].opcode;
	return dot_ops[i].token;
      }
  
  /* See if it is an exponentiation operator.  */

  if (strncmp (tokstart, "**", 2) == 0)
    {
      lexptr += 2;
      yylval.opcode = BINOP_EXP;
      return STARSTAR;
    }

  switch (c = *tokstart)
    {
    case 0:
      return 0;
      
    case ' ':
    case '\t':
    case '\n':
      lexptr++;
      goto retry;
      
    case '\'':
      token = match_string_literal ();
      if (token != 0)
	return (token);
      break;
      
    case '(':
      paren_depth++;
      lexptr++;
      if (strncasecmp(lexptr, "KIND", 4) == 0)
	{
	  /* yacc only supports one lookahead symbols but two are needed to
	   * decide whether a left paranthesis is starting a size/kind or
	   * array dimensions. */
	  lexptr += 4;
	  yylval.opcode = BINOP_END;
	  return KIND;
	}
      return c;
      
    case ')':
      if (paren_depth == 0)
	return 0;
      paren_depth--;
      lexptr++;
      return c;
      
    case ',':
      if (comma_terminates && paren_depth == 0)
	return 0;
      lexptr++;
      return c;
      
    case '.':
      /* Might be a floating point number.  */
      if (lexptr[1] < '0' || lexptr[1] > '9')
	goto symbol;		/* Nope, must be a symbol.  */
      /* FALL THRU into number case.  */
      
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9':
      {
        /* It's a number.  */
	int got_dot = 0, got_e = 0, got_d = 0, toktype;
	char *p = tokstart;
	int hex = input_radix > 10;
	
	if (c == '0' && (p[1] == 'x' || p[1] == 'X'))
	  {
	    p += 2;
	    hex = 1;
	  }
	else if (c == '0' && (p[1]=='t' || p[1]=='T'
			      || p[1]=='d' || p[1]=='D'))
	  {
	    p += 2;
	    hex = 0;
	  }
	
	for (;; ++p)
	  {
	    if (!hex && !got_e && (*p == 'e' || *p == 'E'))
	      got_dot = got_e = 1;
	    else if (!hex && !got_d && (*p == 'd' || *p == 'D'))
	      got_dot = got_d = 1;
	    else if (!hex && !got_dot && *p == '.')
	      got_dot = 1;
	    else if (((got_e && (p[-1] == 'e' || p[-1] == 'E'))
		     || (got_d && (p[-1] == 'd' || p[-1] == 'D')))
		     && (*p == '-' || *p == '+'))
	      /* This is the sign of the exponent, not the end of the
		 number.  */
	      continue;
	    /* We will take any letters or digits.  parse_number will
	       complain if past the radix, or if L or U are not final.  */
	    else if ((*p < '0' || *p > '9')
		     && ((*p < 'a' || *p > 'z')
			 && (*p < 'A' || *p > 'Z')))
	      break;
	  }
	toktype = parse_number (tokstart, p - tokstart, got_dot|got_e|got_d,
				&yylval);
        if (toktype == ERROR)
          {
	    char *err_copy = (char *) alloca (p - tokstart + 1);
	    
	    memcpy (err_copy, tokstart, p - tokstart);
	    err_copy[p - tokstart] = 0;
	    error (_("Invalid number \"%s\"."), err_copy);
	  }
	lexptr = p;
	return toktype;
      }
      
    case '-':
      if (tokstart[1] == '>'){
	lexptr+=2;
	return ARROW;
      }
    case '+':
    case '*':
    case '/':
    case '%':
    case '|':
    case '&':
    case '^':
    case '~':
    case '!':
    case '@':
    case '<':
    case '>':
    case '[':
    case ']':
    case '?':
      lexptr++;
      return c;
    case ':':
      if (tokstart[1] == ':'){
	lexptr+=2;
	return COLONCOLON;
      }
    case '=':
    case '{':
    case '}':
    symbol:
      lexptr++;
      return c;
    }
  
  if (!(c == '_' || c == '$' || c ==':'
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
    /* We must have come across a bad character (e.g. ';').  */
    error (_("Invalid character '%c' in expression."), c);
  
  namelen = 0;
  for (c = tokstart[namelen];
       (c == '_' || c == '$' || c == ':' || (c >= '0' && c <= '9')
	|| (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')); 
       c = tokstart[++namelen]);
  
  /* The token "if" terminates the expression and is NOT 
     removed from the input stream.  */
  
  if (namelen == 2 && tokstart[0] == 'i' && tokstart[1] == 'f')
    return 0;

  /* For the same reason (breakpoint conditions), "thread N"
     terminates the expression.  "thread" could be an identifier, but
     an identifier is never followed by a number without intervening
     punctuation.  "task" is similar.  Handle abbreviations of these,
     similarly to breakpoint.c:find_condition_and_thread.  */
  if (namelen >= 1
      && (strncmp (tokstart, "thread", namelen) == 0
	  || strncmp (tokstart, "task", namelen) == 0
	  || strncmp (tokstart, "inf", namelen) == 0)
      && (tokstart[namelen] == ' ' || tokstart[namelen] == '\t'))
    {
      char *p = tokstart + namelen + 1;
      while (*p == ' ' || *p == '\t')
	p++;
      if (*p >= '0' && *p <= '9')
	return 0;
    }
  
  lexptr += namelen;
  
  /* Catch specific keywords.  */
  
  for (i = 0; f77_keywords[i].operator != NULL; i++)
    if (strlen (f77_keywords[i].operator) == namelen
	&& strncasecmp (tokstart, f77_keywords[i].operator, namelen) == 0)
      {
	/* 	lexptr += strlen(f77_keywords[i].operator); */ 
	yylval.opcode = f77_keywords[i].opcode;
	return f77_keywords[i].token;
      }

  for (i = 0; i < sizeof intrinsics / sizeof intrinsics[0]; i++)
    if (strncasecmp (tokstart, intrinsics[i].operator, strlen(intrinsics[i].operator)) == 0
 	&& strlen(intrinsics[i].operator) == namelen)
      {
        yylval.opcode = intrinsics[i].opcode;
        return intrinsics[i].token;
      }

  yylval.sval.ptr = tokstart;
  yylval.sval.length = namelen;
  
  if (*tokstart == '$')
    {
      write_dollar_variable (yylval.sval);
      return VARIABLE;
    }

  /* Use token-type TYPENAME for symbols that happen to be defined
     currently as names of types; NAME for other symbols.
     The caller is not constrained to care about the distinction.  */
  {
    char *tmp = copy_name (yylval.sval);
    struct symbol *sym;
    struct field_of_this_result is_a_field_of_this;
    int hextype;
    
    /* Initialize this in case we *don't* use it in this call; that
       way we can refer to it unconditionally below.  */
    memset (&is_a_field_of_this, 0, sizeof (is_a_field_of_this));

    sym = lookup_symbol (tmp, expression_context_block,
			 VAR_DOMAIN,
			 parse_language->la_language == language_cplus
			 ? &is_a_field_of_this : NULL);
    if (sym && SYMBOL_CLASS (sym) == LOC_TYPEDEF)
      {
	yylval.tsym.type = SYMBOL_TYPE (sym);
	return TYPENAME;
      }
    yylval.tsym.type
      = language_lookup_primitive_type_by_name (parse_language,
						parse_gdbarch, tmp);
    if (yylval.tsym.type != NULL)
      return TYPENAME;
    
    /* Input names that aren't symbols but ARE valid hex numbers,
       when the input radix permits them, can be names or numbers
       depending on the parse.  Note we support radixes > 16 here.  */
    if (!sym
	&& ((tokstart[0] >= 'a' && tokstart[0] < 'a' + input_radix - 10)
	    || (tokstart[0] >= 'A' && tokstart[0] < 'A' + input_radix - 10)))
      {
 	YYSTYPE newlval;	/* Its value is ignored.  */
	hextype = parse_number (tokstart, namelen, 0, &newlval);
	if (hextype == INT)
	  {
	    yylval.ssym.sym = sym;
	    yylval.ssym.is_a_field_of_this = is_a_field_of_this.type != NULL;
	    return NAME_OR_INT;
	  }
      }
    
    /* Any other kind of symbol */
    yylval.ssym.sym = sym;
    yylval.ssym.is_a_field_of_this = is_a_field_of_this.type != NULL;
    return NAME;
  }
}

void
yyerror (char *msg)
{
  if (prev_lexptr)
    lexptr = prev_lexptr;

  error (_("A %s in expression, near `%s'."), (msg ? msg : "error"), lexptr);
}
