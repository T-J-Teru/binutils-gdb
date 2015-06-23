/* BFD back-end for verilog hex memory dump files.
   Copyright (C) 2009-2015 Free Software Foundation, Inc.
   Written by Anthony Green <green@moxielogic.com>

   This file is part of BFD, the Binary File Descriptor library.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street - Fifth Floor, Boston,
   MA 02110-1301, USA.  */


/* SUBSECTION
	Verilog hex memory file handling

   DESCRIPTION

	Verilog hex memory files cannot hold anything but addresses
	and data, so that's all that we implement.

	The syntax of the text file is described in the IEEE standard
	for Verilog.  Briefly, the file contains two types of tokens:
	data and optional addresses.  The tokens are separated by
	whitespace and comments.  Comments may be single line or
	multiline, using syntax similar to C++.  Addresses are
	specified by a leading "at" character (@) and are always
	hexadecimal strings.  Data and addresses may contain
	underscore (_) characters.

	If no address is specified, the data is assumed to start at
	address 0.  Similarly, if data exists before the first
	specified address, then that data is assumed to start at
	address 0.


   EXAMPLE
	@1000
        01 ae 3f 45 12

   DESCRIPTION
	@1000 specifies the starting address for the memory data.
	The following characters describe the 5 bytes at 0x1000.  */


#include "sysdep.h"
#include "bfd.h"
#include "libbfd.h"
#include "libiberty.h"
#include "safe-ctype.h"

/* For MRK3 the file format is slightly different to the one defined here.
   Without an "official" verilog specification I am not going to try and
   push these changes upstream.  All MRK3 specific changes then are guarded
   with the following define.

   The format changes are as follows:

   - The addresses written to output files are 32-bit, despite mrk3 using
     64-bit elf format.  A result of this is that it is harder to reload a
     verilog DAT file, a base address must always be provided.

   - The address is written onto the same line as the data (at the start
     of the line), and an address is written on every line rather than
     just at the start of the section.  As a result the loading code
     attempts to merge adjacent lines into a single section if the
     addresses are such that the content forms a continuous block.  */

#define MRK3_VERILOG_HACKS

/* Macros for converting between hex and binary.  */

static const char digs[] = "0123456789ABCDEF";

#define NIBBLE(x)    hex_value(x)
#define HEX(buffer) ((NIBBLE ((buffer)[0])<<4) + NIBBLE ((buffer)[1]))
#define TOHEX(d, x) \
	d[1] = digs[(x) & 0xf]; \
	d[0] = digs[((x) >> 4) & 0xf];

/* When writing a verilog memory dump file, we write them in the order
   in which they appear in memory. This structure is used to hold them
   in memory.  */

struct verilog_data_list_struct
{
  struct verilog_data_list_struct *next;
  bfd_byte * data;
  bfd_vma where;
  bfd_size_type size;
};

typedef struct verilog_data_list_struct verilog_data_list_type;

/* The verilog tdata information.  */

typedef struct verilog_data_struct
{
  verilog_data_list_type *head;
  verilog_data_list_type *tail;
}
tdata_type;

static bfd_boolean
verilog_set_arch_mach (bfd *abfd, enum bfd_architecture arch, unsigned long mach)
{
  if (arch != bfd_arch_unknown)
    return bfd_default_set_arch_mach (abfd, arch, mach);

  abfd->arch_info = & bfd_default_arch_struct;
  return TRUE;
}

/* We have to save up all the outpu for a splurge before output.  */

static bfd_boolean
verilog_set_section_contents (bfd *abfd,
			      sec_ptr section,
			      const void * location,
			      file_ptr offset,
			      bfd_size_type bytes_to_do)
{
  tdata_type *tdata = abfd->tdata.verilog_data;
  verilog_data_list_type *entry;

  entry = (verilog_data_list_type *) bfd_alloc (abfd, sizeof (* entry));
  if (entry == NULL)
    return FALSE;

  if (bytes_to_do
      && (section->flags & SEC_ALLOC)
      && (section->flags & SEC_LOAD))
    {
      bfd_byte *data;

      data = (bfd_byte *) bfd_alloc (abfd, bytes_to_do);
      if (data == NULL)
	return FALSE;
      memcpy ((void *) data, location, (size_t) bytes_to_do);

      entry->data = data;
      entry->where = section->lma + offset;
      entry->size = bytes_to_do;

      /* Sort the records by address.  Optimize for the common case of
	 adding a record to the end of the list.  */
      if (tdata->tail != NULL
	  && entry->where >= tdata->tail->where)
	{
	  tdata->tail->next = entry;
	  entry->next = NULL;
	  tdata->tail = entry;
	}
      else
	{
	  verilog_data_list_type **look;

	  for (look = &tdata->head;
	       *look != NULL && (*look)->where < entry->where;
	       look = &(*look)->next)
	    ;
	  entry->next = *look;
	  *look = entry;
	  if (entry->next == NULL)
	    tdata->tail = entry;
	}
    }
  return TRUE;
}

static bfd_boolean
verilog_write_address (bfd *abfd, bfd_vma address)
{
  char buffer[20];
  char *dst = buffer;
  bfd_size_type wrlen;

  /* Write the address.  */
  *dst++ = '@';
#if defined BFD_HOST_64_BIT && !defined MRK3_VERILOG_HACKS
  TOHEX (dst, (address >> 56));
  dst += 2;
  TOHEX (dst, (address >> 48));
  dst += 2;
  TOHEX (dst, (address >> 40));
  dst += 2;
  TOHEX (dst, (address >> 32));
  dst += 2;
#endif
  TOHEX (dst, (address >> 24));
  dst += 2;
  TOHEX (dst, (address >> 16));
  dst += 2;
  TOHEX (dst, (address >> 8));
  dst += 2;
  TOHEX (dst, (address));
  dst += 2;

#ifdef MRK3_VERILOG_HACKS
  *dst++ = ' ';
#else
  *dst++ = '\r';
  *dst++ = '\n';
#endif

  wrlen = dst - buffer;

  return bfd_bwrite ((void *) buffer, wrlen, abfd) == wrlen;
}

/* Write a record of type, of the supplied number of bytes. The
   supplied bytes and length don't have a checksum. That's worked out
   here.  */

static bfd_boolean
verilog_write_record (bfd *abfd,
		      const bfd_byte *data,
		      const bfd_byte *end)
{
  char buffer[50];
  const bfd_byte *src = data;
  char *dst = buffer;
  bfd_size_type wrlen;

  /* Write the data.  */
  for (src = data; src < end; src++)
    {
      TOHEX (dst, *src);
      dst += 2;
      *dst++ = ' ';
    }
  *dst++ = '\r';
  *dst++ = '\n';
  wrlen = dst - buffer;

  return bfd_bwrite ((void *) buffer, wrlen, abfd) == wrlen;
}

static bfd_boolean
verilog_write_section (bfd *abfd,
		       tdata_type *tdata ATTRIBUTE_UNUSED,
		       verilog_data_list_type *list)
{
  unsigned int octets_written = 0;
  bfd_byte *location = list->data;

#ifndef MRK3_VERILOG_HACKS
  verilog_write_address (abfd, list->where);
#endif
  while (octets_written < list->size)
    {
      unsigned int octets_this_chunk = list->size - octets_written;

      if (octets_this_chunk > 16)
	octets_this_chunk = 16;

#ifdef MRK3_VERILOG_HACKS
      verilog_write_address (abfd, list->where + octets_written);
#endif
      if (! verilog_write_record (abfd,
				  location,
				  location + octets_this_chunk))
	return FALSE;

      octets_written += octets_this_chunk;
      location += octets_this_chunk;
    }

  return TRUE;
}

static bfd_boolean
verilog_write_object_contents (bfd *abfd)
{
  tdata_type *tdata = abfd->tdata.verilog_data;
  verilog_data_list_type *list;

  /* Now wander though all the sections provided and output them.  */
  list = tdata->head;

  while (list != (verilog_data_list_type *) NULL)
    {
      if (! verilog_write_section (abfd, tdata, list))
	return FALSE;
      list = list->next;
    }
  return TRUE;
}

/* Initialize by filling in the hex conversion array.  */

static void
verilog_init (void)
{
  static bfd_boolean inited = FALSE;

  if (! inited)
    {
      inited = TRUE;
      hex_init ();
    }
}

/* Set up the verilog tdata information.  */

static bfd_boolean
verilog_mkobject (bfd *abfd)
{
  tdata_type *tdata;

  verilog_init ();

  tdata = (tdata_type *) bfd_alloc (abfd, sizeof (tdata_type));
  if (tdata == NULL)
    return FALSE;

  abfd->tdata.verilog_data = tdata;
  tdata->head = NULL;
  tdata->tail = NULL;

  return TRUE;
}

/* Read a byte from a verilog hex file.  Set *ERRORPTR if an error
   occurred.  Return EOF on error or end of file.  */

static int
verilog_get_byte (bfd *abfd, bfd_boolean *errorptr)
{
  bfd_byte c;

  if (bfd_bread (&c, (bfd_size_type) 1, abfd) != 1)
    {
      if (bfd_get_error () != bfd_error_file_truncated)
	*errorptr = TRUE;
      return EOF;
    }

  return (int) (c & 0xff);
}

/* Helper function used when reading a Verilog file.  Having already read a
   '/' character, which can only indicate the start of a comment, process
   the remainder of the comment, either a single like '//' comment, or a
   block comment in C/C++ style.  Return TRUE if the comment is parsed
   successfully, otherwise return FALSE.  */

static bfd_boolean
verilog_skip_comment (bfd *abfd)
{
  int c;
  bfd_boolean error = FALSE;

  /* Have already seen first '/' now looking for rest of the comment.  */
  c = verilog_get_byte (abfd, &error);
  switch (c)
    {
    default:
    case EOF:
      return FALSE;

    case '/':
      /* Single line comment.  */
      do
        {
          c = verilog_get_byte (abfd, &error);
          if (error)
            return FALSE;
          if (c == '\n' || c == EOF)
            return TRUE;
        }
      while (TRUE);

    case '*':
      /* Multiline comment.  */
      do
        {
          c = verilog_get_byte (abfd, &error);
          if (error || c == EOF)
            return FALSE;
          if (c == '*')
            {
              /* Is next character '/'?  */
              c = verilog_get_byte (abfd, &error);
              if (error || c == EOF)
                return FALSE;
              else if (c == '/')
                return TRUE;
            }
        }
      while (TRUE);
    }

  return FALSE;
}

static bfd_boolean
verilog_read_address (bfd *abfd, bfd_vma *address)
{
  bfd_boolean error = FALSE;
  char buffer [17], *dst;
  int c;
  unsigned long value;

  dst = buffer;
  while ((c = verilog_get_byte (abfd, &error)) != EOF)
    {
      if (error)
        return FALSE;
      else if  (!ISXDIGIT (c))
        break;
      *dst = (char) c;
      dst++;
    }

  if ((dst - buffer) == 0)
    return FALSE;

  *dst = '\0';
  value = strtoul (buffer, NULL, 16);
  *address = (bfd_vma) value;
  return TRUE;
}

/* Read the verilog hex file and turn it into sections.  We create a new
   section for each contiguous set of bytes.  */

static bfd_boolean
verilog_scan (bfd *abfd)
{
  int c;
  bfd_boolean error = FALSE;
  bfd_vma address = 0;
  asection *sec = NULL;

  if (bfd_seek (abfd, (file_ptr) 0, SEEK_SET) != 0)
      goto error_return;

  while ((c = verilog_get_byte (abfd, &error)) != EOF)
    {
      /* Skip whitespace.  */
      if (ISSPACE (c))
        continue;

      if (c == '/')
        {
          if (!verilog_skip_comment (abfd))
            goto error_return;
          continue;
        }

      if (c == '@')
        {
          char secbuf[20];
          char *secname;
          bfd_size_type amt;
          flagword flags;
          file_ptr pos;

          if (!verilog_read_address (abfd, &address))
            goto error_return;

          if (sec
#ifdef MRK3_VERILOG_HACKS
              && (sec->vma + sec->size) != address
#endif
              )
            sec = NULL;

          if (sec == NULL)
            {
              pos = bfd_tell (abfd) - 1;
              sprintf (secbuf, ".sec%d", bfd_count_sections (abfd) + 1);
              amt = strlen (secbuf) + 1;
              secname = (char *) bfd_alloc (abfd, amt);
              strcpy (secname, secbuf);
              flags = SEC_HAS_CONTENTS | SEC_LOAD | SEC_ALLOC;
              sec = bfd_make_section_with_flags (abfd, secname, flags);
              sec->vma = address;
              sec->lma = address;
              sec->size = 0;
              sec->filepos = pos;
            }

          continue;
        }

      if (ISXDIGIT (c))
        {
          int buf[2] = { c, 0 };
          buf [1] = verilog_get_byte (abfd, &error);
          if (error || buf [1]  == EOF || !ISXDIGIT (buf [1]) || sec == NULL)
            goto error_return;

          sec->size++;
          continue;
        }

      /* Unknown input.  */
      goto error_return;
    }

  if (error)
    goto error_return;

  if (sec)
    sec = NULL;
  return TRUE;
 error_return:
  if (sec)
    sec = NULL;
  return FALSE;
}

/* Check whether an existing file is a verilog hex file.  */

static const bfd_target *
verilog_object_p (bfd *abfd)
{
  void * tdata_save;

  verilog_init ();

  tdata_save = abfd->tdata.any;
  if (!verilog_mkobject (abfd) || ! verilog_scan (abfd))
    {
      if (abfd->tdata.any != tdata_save && abfd->tdata.any != NULL)
	bfd_release (abfd, abfd->tdata.any);
      abfd->tdata.any = tdata_save;
      return NULL;
    }

  return abfd->xvec;
}

/* Read the contents of a section in a Verilog Hex file.  */

static bfd_boolean
verilog_read_section (bfd *abfd, asection *section, bfd_byte *contents)
{
  int c;
  bfd_byte *dst;
  bfd_boolean error = FALSE;

  if (bfd_seek (abfd, section->filepos, SEEK_SET) != 0)
    goto error_return;

  dst = contents;
  while ((c = verilog_get_byte (abfd, &error)) != EOF)
    {
      /* Skip whitespace.  */
      if (ISSPACE (c))
        continue;

      if (c == '/')
        {
          if (!verilog_skip_comment (abfd))
            goto error_return;
          continue;
        }

      if (c == '@')
        {
#ifdef MRK3_VERILOG_HACKS
          bfd_vma address = 0;

          if (!verilog_read_address (abfd, &address))
            goto error_return;

          if (section->vma + (dst - contents) != address)
            goto error_return;

          continue;
#else
          break;
#endif
        }

      if (ISXDIGIT (c))
        {
          int buf[2] = { c, 0 }, value;
          buf [1] = verilog_get_byte (abfd, &error);
          if (error || buf [1]  == EOF || !ISXDIGIT (buf [1]))
            goto error_return;
          value = HEX (buf);
          *dst = (bfd_byte) value;
          dst++;
          if ((bfd_size_type) (dst - contents) >= section->size)
            /* We've read everything in the section.  */
            return TRUE;
          continue;
        }
    }

 error_return:
  return FALSE;
}

/* Get the contents of a section in an Verilog Hex file.  */

static bfd_boolean
verilog_get_section_contents (bfd *abfd,
                              asection *section,
                              void * location,
                              file_ptr offset,
                              bfd_size_type count)
{
  if (section->used_by_bfd == NULL)
    {
      section->used_by_bfd = bfd_alloc (abfd, section->size);
      if (section->used_by_bfd == NULL)
	return FALSE;
      if (! verilog_read_section (abfd, section,
                                  (bfd_byte *) section->used_by_bfd))
	return FALSE;
    }

  memcpy (location, (bfd_byte *) section->used_by_bfd + offset,
	  (size_t) count);

  return TRUE;
}

#define	verilog_close_and_cleanup                    _bfd_generic_close_and_cleanup
#define verilog_bfd_free_cached_info                 _bfd_generic_bfd_free_cached_info
#define verilog_new_section_hook                     _bfd_generic_new_section_hook
#define verilog_bfd_is_target_special_symbol         ((bfd_boolean (*) (bfd *, asymbol *)) bfd_false)
#define verilog_bfd_is_local_label_name              bfd_generic_is_local_label_name
#define verilog_get_lineno                           _bfd_nosymbols_get_lineno
#define verilog_find_nearest_line                    _bfd_nosymbols_find_nearest_line
#define verilog_find_inliner_info                    _bfd_nosymbols_find_inliner_info
#define verilog_make_empty_symbol                    _bfd_generic_make_empty_symbol
#define verilog_bfd_make_debug_symbol                _bfd_nosymbols_bfd_make_debug_symbol
#define verilog_read_minisymbols                     _bfd_generic_read_minisymbols
#define verilog_minisymbol_to_symbol                 _bfd_generic_minisymbol_to_symbol
#define verilog_get_section_contents_in_window       _bfd_generic_get_section_contents_in_window
#define verilog_bfd_get_relocated_section_contents   bfd_generic_get_relocated_section_contents
#define verilog_bfd_relax_section                    bfd_generic_relax_section
#define verilog_bfd_gc_sections                      bfd_generic_gc_sections
#define verilog_bfd_merge_sections                   bfd_generic_merge_sections
#define verilog_bfd_is_group_section                 bfd_generic_is_group_section
#define verilog_bfd_discard_group                    bfd_generic_discard_group
#define verilog_section_already_linked               _bfd_generic_section_already_linked
#define verilog_bfd_link_hash_table_create           _bfd_generic_link_hash_table_create
#define verilog_bfd_link_add_symbols                 _bfd_generic_link_add_symbols
#define verilog_bfd_link_just_syms                   _bfd_generic_link_just_syms
#define verilog_bfd_final_link                       _bfd_generic_final_link
#define verilog_bfd_link_split_section               _bfd_generic_link_split_section

const bfd_target verilog_vec =
{
  "verilog",			/* Name.  */
  bfd_target_verilog_flavour,
  BFD_ENDIAN_UNKNOWN,		/* Target byte order.  */
  BFD_ENDIAN_UNKNOWN,		/* Target headers byte order.  */
  (HAS_RELOC | EXEC_P |		/* Object flags.  */
   HAS_LINENO | HAS_DEBUG |
   HAS_SYMS | HAS_LOCALS | WP_TEXT | D_PAGED),
  (SEC_CODE | SEC_DATA | SEC_ROM | SEC_HAS_CONTENTS
   | SEC_ALLOC | SEC_LOAD | SEC_RELOC),	/* Section flags.  */
  0,				/* Leading underscore.  */
  ' ',				/* AR_pad_char.  */
  16,				/* AR_max_namelen.  */
  0,				/* match priority.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Data.  */
  bfd_getb64, bfd_getb_signed_64, bfd_putb64,
  bfd_getb32, bfd_getb_signed_32, bfd_putb32,
  bfd_getb16, bfd_getb_signed_16, bfd_putb16,	/* Hdrs.  */

  {				/* bfd_check_format.  */
    _bfd_dummy_target,
    verilog_object_p,
    _bfd_dummy_target,
    _bfd_dummy_target,
  },
  {
    bfd_false,
    verilog_mkobject,
    bfd_false,
    bfd_false,
  },
  {				/* bfd_write_contents.  */
    bfd_false,
    verilog_write_object_contents,
    bfd_false,
    bfd_false,
  },

  BFD_JUMP_TABLE_GENERIC (verilog),
  BFD_JUMP_TABLE_COPY (_bfd_generic),
  BFD_JUMP_TABLE_CORE (_bfd_nocore),
  BFD_JUMP_TABLE_ARCHIVE (_bfd_noarchive),
  BFD_JUMP_TABLE_SYMBOLS (_bfd_nosymbols),
  BFD_JUMP_TABLE_RELOCS (_bfd_norelocs),
  BFD_JUMP_TABLE_WRITE (verilog),
  BFD_JUMP_TABLE_LINK (_bfd_nolink),
  BFD_JUMP_TABLE_DYNAMIC (_bfd_nodynamic),

  NULL,

  NULL
};
