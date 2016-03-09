/* UPC Debugger Assistant client command parser for GDB, the GNU debugger.

   Contributed by: Gary Funck <gary@intrepid.com>

   Copyright (C) 2007 Free Software Foundation, Inc.

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

%{

#include <ctype.h>
#include "defs.h"
#include "gdb_string.h"
#include "uda-types-client.h"
#include "uda-rmt-utils.h"
#include "uda-client.h"

#define yyparse uda_client_cmd_yyparse
#define yyerror uda_client_cmd_yyerror
#define yylex uda_client_cmd_yylex
#define yyline uda_client_cmd_yyline
#define yytext uda_client_cmd_yytext
#define yyleng uda_client_cmd_yyleng
#define yysstate uda_client_cmd_yysstate
#define yycp uda_client_cmd_yycp
#define yykw uda_client_cmd_yykw
#define yy_cmd_exec uda_client_cmd_exec

#define YYERRORVERBOSE 1
#define YYINITDEPTH 20
#define YYMAXDEPTH  20

extern int yylex(void);
extern void yyerror(const char *msg);

/* Lexer state communication variables  */

typedef enum {scan_cmd, scan_args, scan_data, scan_id, scan_pts} yysstate_t;

static yysstate_t yysstate;

/* #define DEBUG_UDA_RMT_LEX 1 */

%}

%union {
  uda_tword_t val;
  uda_tword_t val2[2];
  uda_tint_t signed_val;
  uda_taddr_t addr_val;
  uda_debugger_pts_t pts;
  char *str;
  uda_binary_data_t data;
}

%token KW_ADDRESS
%token KW_LEN
%token KW_READ
%token KW_TYPE
%token KW_UPC
%token KW_WRITE
%token KW_LOCAL
%token KW_MEMBER
%token KW_THREAD
%token KW_VAR

%token <str> ID_STRING
%token <data> BINARY_DATA
%token <val> HEX_NUM

%type <data> data
%type <val> length
%type <str> symbol
%type <val> thread_num
%type <val> address
%type <str> member_name
%type <val> type_id
%type <str> type_name

%%
/* Begin Grammar */

/* Client commands are commands that the UDA server
   can send back to the client (GDB) to request
   services.  These commands are used to implement
   callback services.  */
client_cmd:
	get_global_var_addr_cmd
	| get_thread_local_addr_cmd
	| get_type_length_cmd
	| get_type_memeber_descr_cmd
	| lookup_type_cmd
	| read_upc_thread_local_mem_cmd
	| write_upc_thread_local_mem_cmd
	;


get_global_var_addr_cmd:
	'q' KW_UPC '.' KW_VAR begin_id_arg1 symbol
	    {
	      const char *symbol = $6;
	      uda_taddr_t address;
	      int status;
	      status = uda_client_get_global_var_addr_cmd (symbol, &address);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux", address);
	      xfree ((void *) symbol);
	    }
	;

lookup_type_cmd:
	'q' KW_UPC '.' KW_TYPE begin_id_arg1 type_name
	    {
	      const char *type_name = $6;
	      uda_tword_t type_id;
	      int status;
	      status = uda_client_lookup_type_cmd (type_name, &type_id);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux", type_id);
	      xfree ((void *) type_name);
	    }
	;

get_type_length_cmd:
	'q' KW_UPC '.' KW_TYPE '.' KW_LEN begin_args type_id
	    {
	      const uda_tword_t type_id = $8;
	      uda_tword_t type_length;
	      int status;
	      status = uda_client_get_type_length_cmd (type_id, &type_length);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux", type_length);
	    }
	;

get_type_memeber_descr_cmd:
	'q' KW_UPC '.' KW_TYPE '.' KW_MEMBER begin_args
	type_id begin_id_arg member_name
	    {
	      const uda_tword_t type_id = $8;
	      const char *member_name = $10;
	      uda_tword_t bit_offset, bit_length, member_type_id;
	      int status;
	      status = uda_client_get_type_member_descr_cmd
	                    (type_id, member_name,
			     &bit_offset, &bit_length, &member_type_id);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux,%lux,%lux",
		                    bit_offset, bit_length, member_type_id);
	      xfree ((void *) member_name);
	    }
	;

get_thread_local_addr_cmd:
	'q' KW_UPC '.' KW_THREAD '.' KW_ADDRESS begin_args
	address ',' thread_num
	    {
	      const uda_taddr_t address = $8;
	      const uda_tword_t thread_num = $10;
	      uda_taddr_t local_address;
	      int status;
	      status = uda_client_get_thread_local_addr_cmd (
	                               address, thread_num, &local_address);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux", local_address);
	    }
	;

read_upc_thread_local_mem_cmd:
	'q' KW_UPC '.' KW_READ '.' KW_LOCAL begin_args
	address ',' thread_num ',' length
	    {
	      const uda_taddr_t addr = $8;
	      const uda_tword_t thread_num = $10;
	      const uda_tword_t length = $12;
	      uda_binary_data_t data;
	      int status;
              status = uda_client_read_local_mem_cmd (
				  addr, thread_num, length, &data);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        {
	          uda_rmt_send_reply ("%*b", data.len, data.bytes);
	          xfree (data.bytes);
	        }
	    }
	;

write_upc_thread_local_mem_cmd:
	'Q' KW_UPC '.' KW_WRITE '.' KW_LOCAL begin_args
	address ',' thread_num begin_binary_data_arg data
	    {
	      const uda_taddr_t address = $8;
	      const uda_tword_t thread_num = $10;
	      const uda_binary_data_t data = $12;
	      uda_tword_t bytes_written;
	      int status;
              status = uda_client_write_local_mem_cmd (
				  address, thread_num, &bytes_written, &data);
	      if (status != uda_ok)
	        uda_rmt_send_status (status);
	      else
	        uda_rmt_send_reply ("%lux", bytes_written);
	      xfree ((void *) data.bytes);
	    }
	;

begin_args:
	':' { yysstate = scan_args; }
	;

begin_binary_data_arg:
	',' { yysstate = scan_data; }
	;

begin_id_arg:
	',' { yysstate = scan_id; }

data:
	BINARY_DATA
	;

length:
	HEX_NUM
	;

symbol:
	ID_STRING
	;

thread_num:
	HEX_NUM
	;

address:
	HEX_NUM
	    { $$ = (uda_taddr_t)$1; }
	;

begin_id_arg1:
	':' { yysstate = scan_id; }

member_name:
	ID_STRING
	;

type_id:
	HEX_NUM
	;

type_name:
	ID_STRING
	;

/* End Grammar */
%%

static const char *yyline;
static const char *yycp;
static const char *yytext;
static size_t yyleng;

typedef struct kw_entry_struct
  {
    const char *kw_id;
    const int kw_token;
  } kw_entry_t;

const kw_entry_t yykw[] =
{
  {"address", KW_ADDRESS},
  {"len", KW_LEN},
  {"local", KW_LOCAL},
  {"member", KW_MEMBER},
  {"read", KW_READ},
  {"thread", KW_THREAD},
  {"type", KW_TYPE},
  {"upc", KW_UPC},
  {"var", KW_VAR},
  {"write", KW_WRITE},
};

#define NUM_KW (sizeof (yykw) / sizeof (kw_entry_t))

#define init_scan(BUF) { yyline = (BUF); yycp = yyline; yysstate = scan_cmd; }
#define nextch() { if (*yycp) { ++yycp; ++yyleng; } }
#define unch() { if (yycp != yyline) { --yycp; --yyleng; } }
#define reset_scan() { yycp = yytext; yyleng = 0; }

int
yylex()
{
  int ch_token;
  yytext = yycp;
  reset_scan ();
  switch (yysstate)
  {
  case scan_cmd:
    {
      /* The first character is a special case, because it
         is used in the protocol to qualify the type of command. */
      if (yycp != yyline && isalpha (*yycp))
        {
	  size_t i;
	  while (isalpha (*yycp)) { nextch(); }
	  for (i = 0; i < NUM_KW; ++i)
	    {
	      if ((strlen (yykw[i].kw_id) == yyleng)
	          && (strncmp (yytext, yykw[i].kw_id, yyleng) == 0))
		{
#ifdef DEBUG_UDA_RMT_LEX
		  printf ("scan: keyword: %s\n", yykw[i].kw_id);
#endif
		  return yykw[i].kw_token;
	        }
	    }
	  reset_scan ();
        }
      break;
    }
  case scan_args:
    {
      if (isxdigit (*yycp))
        {
	  while (isxdigit (*yycp)) { nextch (); }
          yylval.val = uda_decode_hex_word (yytext, yyleng);
#ifdef DEBUG_UDA_RMT_LEX
	  printf ("scan: hex: %lx\n", yylval.val);
#endif
	  return HEX_NUM;
	}
      break;
    }
  case scan_pts:
    {
      if (isxdigit (*yycp))
        {
          const size_t n_bytes = uda_scan_hex_bytes (&yycp);
	  yyleng = yycp - yytext;
          yylval.data.bytes = (unsigned char *)xmalloc(n_bytes);
          yylval.data.len = uda_decode_hex_bytes ((char *) yylval.data.bytes,
	                                           yytext, yyleng);
          yysstate = scan_args;
          return BINARY_DATA;
	}
      break;
    }
  case scan_data:
    {
      const size_t n_bytes = uda_scan_binary_data (&yycp);
      yyleng = yycp - yytext;
      yylval.data.bytes = (unsigned char *)xmalloc(n_bytes);
      yylval.data.len = uda_decode_binary_data ((char *) yylval.data.bytes,
                                                 yytext, yyleng);
      yysstate = scan_args;
#ifdef DEBUG_UDA_RMT_LEX
      printf ("scan: biinary len: %ld\n", yylval.data.len);
#endif
      return BINARY_DATA;
    }
  case scan_id:
    {
      if (isalpha (*yycp) || *yycp == '$' || *yycp == '_')
        {
	  nextch();
          while (isalnum (*yycp) || *yycp == '$' || *yycp == '_'
	         || *yycp == '-' || *yycp == '.' || *yycp == '@')
	    { nextch(); }
          yylval.str = (char *)xmalloc(yyleng + 1);
	  memcpy (yylval.str, yytext, yyleng);
	  yylval.str[yyleng] = '\0';
          yysstate = scan_args;
#ifdef DEBUG_UDA_RMT_LEX
	  printf ("scan: id: %s\n", yylval.str);
#endif
	  return ID_STRING;
	}
      break;
    }
  default:
    abort ();
  }
  ch_token = (*yycp & 0xff);
  nextch ();
#ifdef DEBUG_UDA_RMT_LEX
  printf ("scan: char: '%c'\n", ch_token);
#endif
  return ch_token;
}

void
yyerror (const char *msg)
{
  fprintf (stderr, "%s at '%s'\n", msg, yycp);
}

int
yy_cmd_exec (const char *cmd)
{
  init_scan (cmd);
  return yyparse();
}
