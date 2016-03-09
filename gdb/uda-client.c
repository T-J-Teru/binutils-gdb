/* UPC Debugger Assistant client services implementation
   for GDB, the GNU debugger.

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

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <dlfcn.h>
#include "defs.h"
#include "value.h"
#include "target.h"
#include "gdbcmd.h"
#include "gdbtypes.h"
#include "gdb_string.h"
#include "gdbthread.h"
#include "symtab.h"
#include "inferior.h"
#include "objfiles.h"
#include "uda-types-client.h"
#include "uda-rmt-utils.h"
#include "uda-client.h"
#include "uda-defs.h"
#include "upc-thread.h"

#define A_FMT ((sizeof (long) == 64) ? "%l" : "%L")

#define TYPE_TBL_INIT_ALLOC 256
#define DUMMY_THRNUM ((uda_tword_t)0xffffffff)

static const struct type **type_tbl;
static size_t type_tbl_alloc_size;
static size_t type_tbl_size;
extern uda_tword_t mythread;

typedef struct minimal_symbol *sym_p;

static
CORE_ADDR
lookup_symbol_address (const char *symbol)
{
  const struct minimal_symbol *msym;
  CORE_ADDR retaddr;
  struct obj_section *obj_section;
  msym = lookup_minimal_symbol (symbol, NULL, NULL);
  if (!msym)
    return 0;
  retaddr = SYMBOL_VALUE_ADDRESS(msym);
  obj_section = SYMBOL_OBJ_SECTION (msym);
  if (obj_section && (obj_section->the_bfd_section->flags & SEC_THREAD_LOCAL) != 0)
    retaddr = target_translate_tls_address (obj_section->objfile, retaddr);
  return retaddr;
}

/* Return a unique non-zero integer corresponding to the
   given type.  */

static
int
get_type_id (const struct type *type)
{
  int i;
  for (i = 1; i <= type_tbl_size; ++i)
    {
      if (type_tbl[i-1] == type)
        return i;
    }
  if (type_tbl_size == type_tbl_alloc_size)
    {
      type_tbl_alloc_size *= 2;
      type_tbl = (const struct type **) xrealloc ((char *) type_tbl,
		    type_tbl_alloc_size * sizeof (*type_tbl));
    }
  type_tbl[type_tbl_size++] = type;
  return type_tbl_size;
}

static
int
lookup_type_by_name (const char *type_name)
{
  const struct symbol *sym;
  struct type *type;
  int type_id;
  struct symbol_search *matches;
  /* FIXME: add ^$ anchors to front/back of type_name, so
     that the regex matches only the desired type name.  */
  search_symbols ((char *)type_name, TYPES_DOMAIN,
                   0 /* nfiles */, NULL /* files */, &matches);
  if (!matches)
    return 0;
  /* Arbitrarily use the first match.  */
  sym = matches->symbol;
  free_search_symbols (matches);
  type = SYMBOL_TYPE (sym);
  CHECK_TYPEDEF (type);
  type_id = get_type_id ( type);
  return type_id;
}

static
const struct type *
lookup_type_by_id (const int type_id)
{
  if (type_id < 1 || type_id > type_tbl_size)
    return NULL;
  return type_tbl[type_id-1]; 
}

static
const struct type *
lookup_type_member (const struct type *parent_type, const char *field_name,
		    uda_tword_t *bit_offset, uda_tword_t *bit_length)
{
  struct type *ptype = (struct type *)parent_type;
  struct type *ftype = NULL;
  int n_fields, i;
  *bit_offset = 0;
  *bit_length = 0;
  if (!(ptype && (TYPE_CODE (ptype) == TYPE_CODE_STRUCT
                  || TYPE_CODE (ptype) == TYPE_CODE_UNION)))
    return NULL;
  CHECK_TYPEDEF (ptype);
  n_fields = TYPE_NFIELDS (ptype);
  for (i = 0; i < n_fields; ++i)
    {
      const char *t_field_name = TYPE_FIELD_NAME (ptype, i);
      if (t_field_name && (strcmp (t_field_name, field_name) == 0))
        break;
    }
  if (i >= n_fields)
    return NULL;
  *bit_offset = TYPE_FIELD_BITPOS(ptype, i);
  *bit_length = TYPE_FIELD_BITSIZE(ptype, i);
  ftype = TYPE_FIELD_TYPE(ptype, i);
  CHECK_TYPEDEF (ftype);
  if (!*bit_length)
    *bit_length = TYPE_LENGTH (ftype) * 8;
  return ftype;
}

/* Read LENGTH bytes beginning at ADDR within the UPC thread
   given by THREAD_NUM.  A return status of 0 indicates that
   the read succeeded.  A non-zero status is an errno value
   and indicates that some error occurred during the transfer.  */

static
int
read_local_bytes (const uda_tword_t thread_num,
                  const uda_taddr_t addr,
		  const uda_tword_t length,
		  uda_byte_t *data)
{
  CORE_ADDR local_addr = (CORE_ADDR) addr;
  int status;
  uda_tword_t old_thread_num = 0;
  if (upcsingle)
    {
      if (thread_num != mythread && thread_num != 0 && thread_num != DUMMY_THRNUM)
        return uda_no_information;
    }
  else if (thread_num != DUMMY_THRNUM)
    old_thread_num = upc_thread_set (thread_num);
  status = target_read_memory (local_addr, data, length);
  if (!upcsingle && thread_num != DUMMY_THRNUM)
    upc_thread_restore (old_thread_num);
  return status;
}

/* Write LENGTH bytes beginning at ADDR within the UPC thread
   given by THREAD_NUM.  A return status of 0 indicates that
   the read succeeded.  A non-zero status is an errno value
   and indicates that some error occurred during the transfer.  */

static
int
write_local_bytes (const uda_tword_t thread_num,
                   const uda_taddr_t addr,
		   const uda_tword_t length,
		   const uda_byte_t *data)
{
  CORE_ADDR local_addr = (CORE_ADDR) addr;
  int status;
  uda_tword_t old_thread_num = 0;
  if (upcsingle)
    {
      if (thread_num != mythread && thread_num != 0 && thread_num != DUMMY_THRNUM)
        return uda_no_information;
    }
  else if (thread_num != DUMMY_THRNUM)
    old_thread_num = upc_thread_set (thread_num);
  status = target_write_memory (local_addr, data, length);
  if (!upcsingle && thread_num != DUMMY_THRNUM)
    upc_thread_restore (old_thread_num);
  return status;
}

int
uda_client_get_global_var_addr_cmd (const char *symbol, uda_taddr_t *address)
{
  *address = lookup_symbol_address (symbol);
  if (!*address)
    return uda_no_symbol;
  return uda_ok;
}

int
uda_client_lookup_type_cmd (const char *type_name, uda_tword_t *type_id)
{
  *type_id = lookup_type_by_name (type_name);
  if (!*type_id)
    return uda_no_information;
  return uda_ok;
}

int
uda_client_get_type_length_cmd (const uda_tword_t type_id,
                                uda_tword_t *type_length)
{
  struct type *type = (struct type *) lookup_type_by_id (type_id);
  if (!type)
    return uda_no_information;
  CHECK_TYPEDEF (type);
  *type_length = TYPE_LENGTH (type);
  return uda_ok;
}

int
uda_client_get_type_member_descr_cmd (const uda_tword_t type_id,
                                      const char *member_name,
                                      uda_tword_t *bit_offset,
				      uda_tword_t *bit_length,
				      uda_tword_t *member_type_id)
{
  const struct type *struct_type = lookup_type_by_id (type_id);
  const struct type *member_type;
  if (!struct_type)
    return uda_no_information;
  member_type = lookup_type_member (struct_type, member_name,
                                    bit_offset, bit_length);
  if (!member_type)
    return uda_no_information;
  *member_type_id = get_type_id (member_type);
  return uda_ok;
}

int
uda_client_get_thread_local_addr_cmd (const uda_taddr_t address,
                                      const uda_tword_t thread_num,
				      uda_taddr_t *local_address)
{
  *local_address = address;
  return uda_ok;
}

int
uda_client_read_local_mem_cmd (const uda_taddr_t addr,
                               const uda_tword_t thread_num,
			       const uda_tword_t length,
			       uda_binary_data_t *data)
{
  int status;
  data->bytes = (uda_byte_t *) xmalloc ((size_t) length);
  data->len = length;
  status = read_local_bytes (thread_num, addr, length, data->bytes);
  if (status != 0)
    return uda_read_failed;
  return uda_ok;
}

int
uda_client_write_local_mem_cmd (const uda_taddr_t addr,
                                const uda_tword_t thread_num,
				uda_tword_t *bytes_written,
				const uda_binary_data_t *data)
{
  int status;
  int length = data->len;
  *bytes_written = 0;
  status = write_local_bytes (thread_num, addr, length, data->bytes);
  if (status != 0)
    return uda_write_failed;
  *bytes_written = data->len;
  return uda_ok;
}

int
uda_set_num_threads (const uda_tword_t num_threads)
{
  int status;
  uda_rmt_send_cmd ("Qupc.threads:%ux", num_threads);
  status = uda_rmt_recv_status ();
  return status;
}

int
uda_set_thread_num (const uda_tword_t thread_num)
{
  int status;
  mythread = thread_num;
  uda_rmt_send_cmd ("Qupc.thread:%ux", thread_num);
  status = uda_rmt_recv_status ();
  return status;
}

int
uda_get_num_threads (uda_tword_t *num_threads)
{
  int status;
  uda_rmt_send_cmd ("Qupc.get.threads");
  *num_threads = 0;
  status = uda_rmt_recv_reply ("%lux", num_threads);
  return status;
}

int
uda_get_thread_num (uda_tword_t *thread_num)
{
  int status;
  uda_rmt_send_cmd ("Qupc.get.thread");
  *thread_num = (uda_tword_t) -1;
  status = uda_rmt_recv_reply ("%lux", thread_num);
  mythread = *thread_num;
  return status;
}

int
uda_set_type_sizes_and_byte_order (const uda_target_type_sizes_t targ_info,
                                   const uda_tword_t byte_order)
{
  int status;
  uda_rmt_send_cmd ("Qupc.type.sizes:%ux,%ux,%ux,%ux,%ux,%ux",
                    targ_info.short_size, targ_info.int_size,
                    targ_info.long_size, targ_info.long_long_size,
                    targ_info.pointer_size, byte_order);
  status = uda_rmt_recv_status ();
  return status;
}

int
uda_symbol_to_pts (const uda_tword_t elem_size,
                   const uda_tword_t block_size,
		   const uda_taddr_t addrfield,
		   const char *symbol,
		   uda_debugger_pts_t *pts)
{
  int status;
  memset (pts, '\0', sizeof (*pts));
  uda_rmt_send_cmd ("qupc.sym:%lux,%lux,%lux,%s",
                    elem_size, block_size, addrfield, symbol);
  status = uda_rmt_recv_reply ("%lux,%lux", &pts->addrfield, &pts->thread);
  return status;
}

int
uda_length_of_pts (uda_tword_t block_size,
		   uda_tword_t *pts_len)
{
  int status;
  uda_rmt_send_cmd ("qupc.pts.len:%lux", block_size);
  status = uda_rmt_recv_reply ("%lux", pts_len);
  return status;
}

int
uda_pts_to_addr (const uda_debugger_pts_t *pts,
		 const uda_tword_t block_size,
		 const uda_tword_t elem_size,
		 uda_taddr_t *addr)
{
  int status;
  uda_rmt_send_cmd ("qupc.pts.address:%lux,%lux,%lux,%lux,%lux",
                    pts->addrfield, pts->thread, pts->phase,
		    block_size, elem_size);
  *addr = 0;
  status = uda_rmt_recv_reply ("%lux", addr);
  return status;
}

int
uda_unpack_pts (const size_t packed_pts_len,
                const uda_target_pts_t *packed_pts,
	        const uda_tword_t block_size,
		const uda_tword_t elem_size,
	        uda_debugger_pts_t *pts)
{
  int status;
  uda_rmt_send_cmd ("qupc.pts.unpack:%*X,%lux,%lux",
                    packed_pts_len, packed_pts, block_size, elem_size);
  status = uda_rmt_recv_reply ("%lux,%lux,%lux,%lux",
                               &pts->addrfield, &pts->thread,
                               &pts->phase, &pts->opaque);
  return status;
}

int
uda_pack_pts (const uda_taddr_t addrfield,
              const uda_tword_t thread,
              const uda_tword_t phase,
              const uda_tword_t block_size,
	      const uda_tword_t elem_size,
	      size_t *packed_pts_len,
              uda_target_pts_t *packed_pts)
{
  int status;
  uda_rmt_send_cmd ("qupc.pts.pack:%lux,%lux,%lux,%lux,%lux",
                    addrfield, thread, phase, block_size, elem_size);
  status = uda_rmt_recv_reply ("%*X", packed_pts_len, &packed_pts);
  return status;
}

int
uda_calc_pts_index_add (const uda_debugger_pts_t *pts,
		        const uda_tint_t index,
		        const uda_tword_t elem_size,
		        const uda_tword_t block_size,
		        uda_debugger_pts_t *result)
{
  int status;
  uda_rmt_send_cmd ("qupc.pts.index:%lux,%lux,%lux,%lx,%lux,%lux",
                    pts->addrfield, pts->thread, pts->phase,
		    index, elem_size, block_size);
  status = uda_rmt_recv_reply ("%lux,%lux,%lux",
		               &result->addrfield, &result->thread,
			       &result->phase);
  return status;
}

int
uda_calc_pts_diff      (const uda_debugger_pts_t *pts_1,
                        const uda_debugger_pts_t *pts_2,
		        const uda_tword_t elem_size,
		        const uda_tword_t block_size,
		        uda_tint_t *result)
{
  int status;
  uda_rmt_send_cmd (
    "qupc.pts.diff:%lux,%lux,%lux,%lux,%lux,%lux,%lux,%lux",
                    pts_1->addrfield, pts_1->thread, pts_1->phase,
                    pts_2->addrfield, pts_2->thread, pts_2->phase,
		    elem_size, block_size);
  status = uda_rmt_recv_reply ("%lx", result);
  return status;
}

int
uda_read_shared_mem (const uda_taddr_t addrfield,
                     const uda_tword_t thread,
		     const uda_tword_t phase,
                     uda_tword_t block_size,
                     uda_tword_t element_size,
		     const uda_tword_t length,
		     uda_binary_data_t *data)
{
  int status;  
  uda_rmt_send_cmd ("qupc.read.shared:%lux,%lux,%lux,%lux,%lux,%lux",
		    addrfield, thread, phase,
		    block_size, element_size, length);
  status = uda_rmt_recv_reply ("%*b", &data->len, &data->bytes);
  return status;
}

int
uda_write_shared_mem (const uda_taddr_t addrfield,
                      const uda_tword_t thread,
		      const uda_tword_t phase,
                      uda_tword_t block_size,
                      uda_tword_t element_size,
		      const uda_tword_t length,
		      uda_tword_t *bytes_written,
		      const uda_binary_data_t *bytes)
{
  int status;
  uda_rmt_send_cmd ("Qupc.write.shared:%lux,%lux,%lux,%lux,%lux,%*b",
                    addrfield, thread, phase,
		    block_size, element_size, (size_t) length, bytes);
  *bytes_written = 0;
  status = uda_rmt_recv_reply ("%lux", bytes_written);
  return status;
}

void
uda_client_connect (const char *service_name)
{
  int connect_fd;
  FILE *c_in, *c_out;
  struct sockaddr_un saun;
  socklen_t len;
  int status;
  /* Create a UNIX domain socket. */
  if ((connect_fd = socket (AF_UNIX, SOCK_STREAM, 0)) < 0)
    {
      perror_with_name ("socket");
    }
  /* Connect to named socket.  */
  saun.sun_family = AF_UNIX;
  strcpy (saun.sun_path, service_name);
  len = sizeof (saun.sun_family) + strlen (saun.sun_path);
  if (connect (connect_fd, (struct sockaddr *) &saun, len) < 0)
    {
      perror_with_name ("connect");
    }
  c_in = fdopen (connect_fd, "r");
  if (!c_in)
    {
      perror_with_name ("fdopen of c_in failed");
    }
  c_out = fdopen (connect_fd, "w");
  if (!c_out)
    {
      perror_with_name ("fdopen of c_out failed");
    }
  setlinebuf (c_out);
  setvbuf (c_in, (char *) NULL, _IONBF, 0);
  uda_rmt_init (c_in, c_out, uda_client_cmd_exec);
  uda_rmt_send_cmd ("Qupc.init");
  status = uda_rmt_recv_status ();
  if (status != uda_ok)
    {
      if (status == uda_bad_assistant)
	error (_("UDA initialisation failed.\nFailed to load the assistant plugin.\nCheck the UDA_PLUGIN_LIBRARY environment variable."));
      else
	error (_("UDA initialisation failed."));
    }
}

void
init_uda_client(uda_callouts_p callouts)
{
  callouts->uda_set_num_threads = &uda_set_num_threads;
  callouts->uda_set_thread_num = &uda_set_thread_num;
  callouts->uda_get_num_threads = &uda_get_num_threads;
  callouts->uda_get_thread_num = &uda_get_thread_num;
  callouts->uda_set_type_sizes_and_byte_order = &uda_set_type_sizes_and_byte_order;
  callouts->uda_symbol_to_pts = &uda_symbol_to_pts;
  callouts->uda_length_of_pts = &uda_length_of_pts;
  callouts->uda_unpack_pts = &uda_unpack_pts;
  callouts->uda_pack_pts = &uda_pack_pts;
  callouts->uda_calc_pts_index_add = &uda_calc_pts_index_add;
  callouts->uda_calc_pts_diff = &uda_calc_pts_diff;
  callouts->uda_pts_to_addr = &uda_pts_to_addr;
  callouts->uda_read_shared_mem = &uda_read_shared_mem;
  callouts->uda_write_shared_mem = &uda_write_shared_mem; 
}

extern void _initialize_uda_client (void);

void
_initialize_uda_client (void)
{
  type_tbl = (const struct type **) xmalloc (TYPE_TBL_INIT_ALLOC * sizeof (*type_tbl));
  type_tbl_size  = 0;
  type_tbl_alloc_size = TYPE_TBL_INIT_ALLOC;
}
