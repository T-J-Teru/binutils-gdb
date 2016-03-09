/* UPC Debugger Assistant remote protocol utilities for GDB, the GNU debugger.

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

#include <ctype.h>
#include "defs.h"
#include "gdb_assert.h"
#include "gdb_string.h"
#include "gdbcmd.h"
#include "uda-types.h"
#include "uda-rmt-utils.h"

static FILE *uda_rmt_in;
static FILE *uda_rmt_out;
static uda_rmt_cmd_fp_t uda_rmt_cmd_exec;
static uda_target_type_sizes_t uda_target_type_sizes;
static int uda_target_pts_has_opaque;
static int uda_rmt_is_big_end;
static int uda_target_is_big_end;
static int debug_uda = 0;

extern void _initialize_uda_rmt_utils (void);
static void uda_rmt_be_cvt (gdb_byte *, const gdb_byte *, const size_t);
static void show_debug_uda (struct ui_file *file, int from_tty,
		            struct cmd_list_element *c, const char *value);


void
uda_rmt_send_status (const int status)
{
  char msg_buf[4];
  const char *msg;
  if (status == uda_unimplemented)
    msg = "";
  else if (status == uda_ok)
    msg = "OK";
  else
    {
      sprintf (msg_buf, "E%02x", status & 0xff);
      msg = msg_buf;
    }
  if (debug_uda)
    printf ("--> status: +%s\n", msg);
  fputc ('+', uda_rmt_out);
  fputs (msg, uda_rmt_out);
  fputc ('\n', uda_rmt_out);
}

void
uda_rmt_send_reply (const char *fmt, ...)
{
  va_list ap;
  uda_string_t msg;
  va_start (ap, fmt);
  uda_rmt_vformat_msg (msg, fmt, ap);
  va_end (ap);
  if (debug_uda)
    printf ("--> reply: +%s\n", msg);
  fputc ('+', uda_rmt_out);
  fputs (msg, uda_rmt_out);
  fputc ('\n', uda_rmt_out);
}

void
uda_rmt_send_cmd (const char *fmt, ...)
{
  va_list ap;
  uda_string_t msg;
  va_start (ap, fmt);
  uda_rmt_vformat_msg (msg, fmt, ap);
  va_end (ap);
  if (debug_uda)
    printf ("--> command: $%s\n", msg);
  fputc ('$', uda_rmt_out);
  fputs (msg, uda_rmt_out);
  fputc ('\n', uda_rmt_out);
}

int
uda_rmt_recv_reply (const char *fmt, ...)
{
  int slen;
  va_list ap;
  uda_string_t reply;
  while (fgets (reply, sizeof (reply), uda_rmt_in))
    {
      slen = strlen (reply);
      if (reply[slen - 1] == '\n')
	reply[--slen] = '\0';
      /* Replies are prefixed with '+'.  Callback commands
         (processed by the UDA client) are prefixed by '$'.  */
      if (reply[0] == '+')
	{
	  if (!strcmp (reply + 1, "OK"))
	    {
	      if (debug_uda)
		printf ("<-- status: %s\n", reply);
	      return uda_ok;
	    }
	  else if (reply[1] == 'E')
	    {
	      /* Error code */
	      int status;
	      if (debug_uda)
		printf ("<-- error: %s\n", reply);
	      gdb_assert (strlen (reply) == 4);
	      gdb_assert (isxdigit (reply[2]) && isxdigit (reply[3]));
	      sscanf (reply, "+E%2x", &status);
	      return status;
	    }
	  else if (!reply[1])
	    {
	      /* Empty reply indicates no action taken.  */
	      if (debug_uda)
		printf ("<-- status: no information\n");
	      return uda_no_information;
	    }
	  else
	    {
	      /* Reply has data payload.  */
	      if (debug_uda)
		printf ("<-- reply: %s\n", reply);
	      va_start (ap, fmt);
	      (void ) uda_rmt_vscan_msg (reply + 1, fmt, ap);
	      va_end (ap);
	      return uda_ok;
	    }
	}
      else if (reply[0] == '$')
	{
	  if (debug_uda)
	    printf ("<-- command: %s\n", reply);
	  /* Parse and execute the command. */
	  gdb_assert (uda_rmt_cmd_exec);
	  (*uda_rmt_cmd_exec) (reply + 1);
	}
      else
	abort ();
    }
  if (errno)
    perror ("receive reply");
  else
    fprintf (stderr, "receive reply: unexpected EOF\n");
  abort ();
}

void
uda_rmt_format_msg (char *buf, const char *fmt, ...)
{
  va_list ap;
  va_start (ap, fmt);
  uda_rmt_vformat_msg (buf, fmt, ap);
  va_end (ap);
}

/*
 * The 'fmt' specification is similar to that of
 * printf but has special meanings that relate only
 * to the UDA interface:
 *
 * %s   Arbitrary string of bytes.
 *
 * %*b  Arbitrary sequence of bytes encoded using the remote
 *      protocol binary data encoding, where the number
 *      of bytes is given by the value of an argument.
 *
 * %ux  Unsigned hex 32-bit word, leading zero bytes suppressed
 *      represented in big-endian form.
 *
 * %lux Unsigned hex 64-bit word, leading zero bytes suppressed
 *      represented in big-endian form.
 *
 * %x   Signed hex 32-bit word, leading zero bytes suppressed
 *      represented in big-endian form.  A leading '-'
 *      indicates a negative number.
 *
 * %lx  Signed hex 64-bit word, leading zero bytes suppressed
 *      represented in big-endian form.  A leading '-'
 *      indicates a negative number.
 *
 * %X   Target-specific 'unsigned int' sized value
 *      encoded in target-endian order, as a sequence
 *      of bytes, where all bytes are expressed as two hex digits.
 *
 * %lX  Target-specific 'unsigned long' sized value
 *      encoded in target-endian order, as a sequence
 *      of bytes, where all bytes are expressed as two hex digits.
 *
 * %*X  Arbitrary sequence of hex bytes, where the number
 *      of bytes is given by the value of an argument.
 *
 */

void
uda_rmt_vformat_msg (char *buf, const char *fmt, va_list ap_arg)
{
  va_list ap;
  const char *fp;
  char *bp;
  const char *last_byte = &buf[sizeof (uda_string_t) - 1];
  const int skip_leading_zeros = 1;
  va_copy (ap, ap_arg);
  gdb_assert (fmt);
  for (fp = fmt, bp = buf; *fp; ++fp)
    {
      if (*fp == '%')
	{
	  size_t slen;
	  size_t n_bytes;
	  if (*(++fp) == '%')
	    {
	      gdb_assert (bp < (last_byte - 1));
	      *bp++ = '%';
	    }
	  else if (fp[0] == '*' && fp[1] == 'b')
	    {
	      /* Encoded binary data */
	      uda_string_t encoded_bytes;
	      gdb_byte *bytes;
	      n_bytes = va_arg (ap, int);
	      ++fp;
	      bytes = va_arg (ap, gdb_byte *);
	      uda_encode_binary_data (encoded_bytes, bytes, n_bytes);
	      slen = strlen (encoded_bytes);
	      gdb_assert ((bp + slen) < last_byte);
	      strcpy (bp, encoded_bytes);
	      bp += slen;
	    }
	  else if (*fp == 's')
	    {
	      /* Arbitrary string */
	      char *str = va_arg (ap, char *);
	      slen = strlen (str);
	      gdb_assert ((bp + slen) < last_byte);
	      strcpy (bp, str);
	      bp += slen;
	    }
	  else
	    {
	      uda_string_t hex_buf;
	      int l_flag = 0;
	      int u_flag = 0;
	      if (*fp == 'l')
		{
		  l_flag = 1;
		  ++fp;
		}
	      if (*fp == 'u')
		{
		  u_flag = 1;
		  ++fp;
		}
	      if (*fp == 'x')
		{
		  /* Big-endian format */
		  gdb_byte *src;
		  ULONGEST u64;
		  unsigned int u32;
		  LONGEST i64;
		  int i32;
		  gdb_byte bytes[8];
		  char *hex_p = hex_buf;
		  n_bytes = l_flag ? 8 : 4;
		  if (u_flag)
		    {
		      if (l_flag)
		        {
			  u64 = va_arg (ap, ULONGEST);
			  src = (gdb_byte *) &u64;
			}
		      else
		        {
			  u32 = va_arg (ap, unsigned int);
			  src = (gdb_byte *) &u32;
			}
		    }
		  else
		    {
		      if (l_flag)
			{
			  i64 = va_arg (ap, LONGEST);
			  if (i64 < 0)
			    {
			      *hex_p++ = '-';
			      i64 = -i64;
			    }
			  src = (gdb_byte *) &i64;
			}
		      else
			{
			  i32 = va_arg (ap, int);
			  if (i32 < 0)
			    {
			      *hex_p++ = '-';
			      i32 = -i32;
			    }
			  src = (gdb_byte *) &i32;
			}
		    }
		  uda_rmt_be_cvt (bytes, src, n_bytes);
		  uda_encode_hex_bytes (hex_p, bytes, n_bytes,
					skip_leading_zeros);
		}
	      else if (*fp == 'X' || (fp[0] == '*' && fp[1] == 'X'))
		{
		  /* Target-endian format */
		  gdb_byte *bytes;
		  gdb_assert (!u_flag);
		  if (*fp == '*')
		    {
		      gdb_assert (!l_flag);
		      n_bytes = va_arg (ap, size_t);
		      gdb_assert (n_bytes < sizeof (hex_buf) / 2);
		      ++fp;
		    }
		  else
		    n_bytes = l_flag ? uda_target_type_sizes.long_size
		      : uda_target_type_sizes.int_size;
		  bytes = va_arg (ap, gdb_byte *);
		  uda_encode_hex_bytes (hex_buf, bytes, n_bytes,
					!skip_leading_zeros);
	        }
	      else
		abort ();
	      slen = strlen (hex_buf);
	      gdb_assert ((bp + slen) < (last_byte - 1));
	      strcpy (bp, hex_buf);
	      bp += slen;
	    }
	}
      else
	{
	  gdb_assert (bp < (last_byte - 1));
	  *bp++ = *fp;
	}
    }
  va_end (ap);
  *bp++ = '\0';
}

void
uda_rmt_set_target_info (const uda_target_type_sizes_t *target_sizes,
			 const int target_is_big_end,
			 const int target_pts_has_opaque)
{
  uda_target_type_sizes = *target_sizes;
  uda_target_is_big_end = target_is_big_end;
  uda_target_pts_has_opaque = target_pts_has_opaque;
}

static inline unsigned int
xtoi (const char c)
{
  return (c >= 'A' && c <= 'F') ? (c - 'A' + 10)
    : (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : (c - '0');
}

static inline unsigned int
xtob (const char *s)
{
  return (xtoi (s[0]) << 4) + xtoi (s[1]);
}

ULONGEST
uda_decode_hex_word (const char *s, const size_t n_chars)
{
  ULONGEST v;
  const char *xp;
  size_t xrem;
  /* Ignore leading zeroes. */
  for (xp = s, xrem = n_chars; xrem && *xp == '0'; ++xp, --xrem) /* loop */ ;
  gdb_assert (xrem <= sizeof (ULONGEST) * 2);
  for (v = 0; xrem > 0; --xrem)
    v = (v << 4) | (ULONGEST) xtoi (*xp++);
  return v;
}

/* Scan a string of hex bytes.  Return the number
   of bytes after decoding, and update the scan pointer located by 's'.  */

size_t
uda_scan_hex_bytes (const char **s)
{
  const char *s1 = *s;
  size_t cnt;
  const char *cp;
  for (cp = s1; isxdigit (*cp); ++cp) /* loop */ ;
  cnt = (cp - s1 + 1) / 2;
  *s = cp;
  return cnt;
}

size_t
uda_decode_hex_bytes (char *bytes, const char *s, const size_t n)
{
  char *bp = bytes;
  const char *cp = s;
  size_t rem = n;
  if (*cp && ((rem % 2) != 0))
    {
      *bp++ = xtoi (*cp++);
      --rem;
    }
  for (; rem > 0; cp += 2, rem -= 2)
    *bp++ = xtob (cp);
  return (bp - bytes);
}

void
uda_encode_hex_bytes (char *hex_buf,
		      const gdb_byte * bytes,
		      const size_t n_bytes, const int skip_leading_zeros)
{
  size_t i = 0;
  char *cp;
  if (skip_leading_zeros)
    while ((i < (n_bytes - 1)) && bytes[i] == 0)
      ++i;
  for (cp = hex_buf; i < n_bytes; ++i)
    {
      gdb_byte b = bytes[i];
      unsigned nib1 = (b >> 4) & 0xf;
      unsigned nib2 = b & 0xf;
      *cp++ = (nib1 < 10) ? '0' + nib1 : 'a' + (nib1 - 10);
      *cp++ = (nib2 < 10) ? '0' + nib2 : 'a' + (nib2 - 10);
    }
  *cp = '\0';
}

/* Scan encoded character string or binary data up until the next
   delimiter (which may be '\0', ',', or '\n').  Return the number
   of bytes after decoding, and update the scan pointer located by 's'.  */

size_t
uda_scan_binary_data (const char **s)
{
  size_t cnt;
  const char *cp;
  for (cp = *s, cnt = 0; *cp && *cp != ',' && *cp != '\n';)
    {
      if (cp[0] == '\\' && cp[1] == 'x'
	  && isxdigit (cp[2]) && isxdigit (cp[3]))
	{
	  ++cnt;
	  cp += 4;
	}
      else if (*cp == '\\')
	{
	  ++cnt;
	  cp += 2;
	}
      else if (*cp == '*' && isxdigit (cp[1]) && isxdigit (cp[2]))
	{
	  const gdb_byte rbyte = xtob (cp + 1);
	  const size_t nreps = (rbyte >= 5) ? rbyte : (1 << (8 + rbyte));
	  cnt += nreps - 1;
	  cp += 3;
	}
      else
	{
	  gdb_assert (isprint (*cp) && *cp != '"' && *cp != '\'');
	  ++cnt;
	  ++cp;
	}
    }
  *s = cp;
  return cnt;
}

size_t
uda_decode_binary_data (char *bytes, const char *s, const size_t n)
{
  char *bp;
  const char *cp;
  size_t rem, len, nc;
  for (cp = s, rem = n, bp = bytes, len = 0; rem > 0; cp += nc, rem -= nc)
    {
      if (*cp == '\\')
	{
	  char b;
	  const char ch = cp[1];
	  gdb_assert (rem > 1);
	  if (ch == 'x')
	    {
	      /* Escaped hex */
	      gdb_assert (rem >= 4);
	      nc = 4;
	      b = xtob (cp + 2);
	    }
	  else
	    {
	      /* Escaped single character */
	      gdb_assert (rem >= 2);
	      nc = 2;
	      switch (ch)
	         {
		 case '0': b = '\0';	break;
		 case 'b': b = '\b';	break;
		 case 'f': b = '\f';	break;
		 case 'n': b = '\n';	break;
		 case 'r': b = '\r';	break;
		 case 't': b = '\t';	break;
		 default:  b = ch;
	         }
	    }
	  *bp++ = b;
	}
      else if (*cp == '*' && bp != bytes && rem >= 3)
	{
	  const gdb_byte rbyte = xtob (cp + 1);
	  const size_t nreps = (rbyte >= 5) ? rbyte : (1 << (8 + rbyte));
	  nc = 3;
	  memset (bp, bp[-1], nreps - 1);
	  bp += nreps - 1;
	}
      else
	{
	  nc = 1;
	  *bp++ = *cp;
	}
    }
  len = bp - bytes;
  return len;
}

static const char * const encoded_char[256] = {
    "\\0",   "\\x01", "\\x02", "\\x03", "\\x04", "\\x05", "\\x06", "\\x07",
    "\\b",   "\\t",   "\\n",   "\\x0b", "\\f",   "\\r",   "\\x0e", "\\x0f",
    "\\x10", "\\x11", "\\x12", "\\x13", "\\x14", "\\x15", "\\x16", "\\x17",
    "\\x18", "\\x19", "\\x1a", "\\x1b", "\\x1c", "\\x1d", "\\x1e", "\\x1f",
    " ",     "!",     "\\\"",  "#",     "$",     "%",     "&",     "\\'",
    "(",     ")",     "\\*",   "+",     "\\,",   "-",     ".",     "/",
    "0",     "1",     "2",     "3",     "4",     "5",     "6",     "7",
    "8",     "9",     ":",     ";",     "<",     "=",     ">",     "?",
    "@",     "A",     "B",     "C",     "D",     "E",     "F",     "G",
    "H",     "I",     "J",     "K",     "L",     "M",     "N",     "O",
    "P",     "Q",     "R",     "S",     "T",     "U",     "V",     "W",
    "X",     "Y",     "Z",     "[",     "\\\\",  "]",     "^",     "_",
    "`",     "a",     "b",     "c",     "d",     "e",     "f",     "g",
    "h",     "i",     "j",     "k",     "l",     "m",     "n",     "o",
    "p",     "q",     "r",     "s",     "t",     "u",     "v",     "w",
    "x",     "y",     "z",     "{",     "|",     "}",     "~",     "\\x7f",
    "\\x80", "\\x81", "\\x82", "\\x83", "\\x84", "\\x85", "\\x86", "\\x87",
    "\\x88", "\\x89", "\\x8a", "\\x8b", "\\x8c", "\\x8d", "\\x8e", "\\x8f",
    "\\x90", "\\x91", "\\x92", "\\x93", "\\x94", "\\x95", "\\x96", "\\x97",
    "\\x98", "\\x99", "\\x9a", "\\x9b", "\\x9c", "\\x9d", "\\x9e", "\\x9f",
    "\\xa0", "\\xa1", "\\xa2", "\\xa3", "\\xa4", "\\xa5", "\\xa6", "\\xa7",
    "\\xa8", "\\xa9", "\\xaa", "\\xab", "\\xac", "\\xad", "\\xae", "\\xaf",
    "\\xb0", "\\xb1", "\\xb2", "\\xb3", "\\xb4", "\\xb5", "\\xb6", "\\xb7",
    "\\xb8", "\\xb9", "\\xba", "\\xbb", "\\xbc", "\\xbd", "\\xbe", "\\xbf",
    "\\xc0", "\\xc1", "\\xc2", "\\xc3", "\\xc4", "\\xc5", "\\xc6", "\\xc7",
    "\\xc8", "\\xc9", "\\xca", "\\xcb", "\\xcc", "\\xcd", "\\xce", "\\xcf",
    "\\xd0", "\\xd1", "\\xd2", "\\xd3", "\\xd4", "\\xd5", "\\xd6", "\\xd7",
    "\\xd8", "\\xd9", "\\xda", "\\xdb", "\\xdc", "\\xdd", "\\xde", "\\xdf",
    "\\xe0", "\\xe1", "\\xe2", "\\xe3", "\\xe4", "\\xe5", "\\xe6", "\\xe7",
    "\\xe8", "\\xe9", "\\xea", "\\xeb", "\\xec", "\\xed", "\\xee", "\\xef",
    "\\xf0", "\\xf1", "\\xf2", "\\xf3", "\\xf4", "\\xf5", "\\xf6", "\\xf7",
    "\\xf8", "\\xf9", "\\xfa", "\\xfb", "\\xfc", "\\xfd", "\\xfe", "\\xff"};

void
uda_encode_binary_data (char *encoded_data,
			const gdb_byte *data, const size_t n_bytes)
{
  char *outp = encoded_data;
  size_t n = 0;
  while (n < n_bytes)
    {
      const gdb_byte b = data[n];
      const size_t rem = n_bytes - n;
      size_t cnt;
      /* Count number of occurrences of this character.  */
      for (cnt = 1; cnt < rem && data[n + cnt] == b; )
        ++cnt;
      while (cnt > 0)
	{
	  const char *enc = encoded_char[b];
	  if (b == 'E' && n == 0)
	    {
	      /* An 'E' in the first position has to be escaped,
	         to avoid confusion with an error code return.  */
	      enc = "\\x45";
	    }
	  while (*enc)
	    *outp++ = *enc++;
	  if (cnt < 5)
	    {
	      --cnt;
	      ++n;
	    }
	  else
	    {
	      const gdb_byte rbyte = (cnt >= 4096)    ? 4
	                             : (cnt >= 2048) ? 3
				     : (cnt >= 1024) ? 2
				     : (cnt >= 512)  ? 1
			             : (cnt >= 256)  ? 0 : cnt;
	      const size_t nreps =  (rbyte >= 5) ? rbyte : (1 << (8 + rbyte));
	      const unsigned nib1 = (rbyte >> 4) & 0xf;
	      const unsigned nib2 = rbyte & 0xf;
	      *outp++ = '*';
	      *outp++ = (nib1 < 10) ? '0' + nib1 : 'a' + nib1 - 10;
	      *outp++ = (nib2 < 10) ? '0' + nib2 : 'a' + nib2 - 10;
	      cnt -= nreps;
	      n += nreps;
	    }
	}
    }
  *outp = '\0';
}

int
uda_rmt_scan_msg (const char *msg, const char *fmt, ...)
{
  int ret;
  va_list ap;
  va_start (ap, fmt);
  ret = uda_rmt_vscan_msg (msg, fmt, ap);
  va_end (ap);
  return ret;
}

/*
 * Scan 'msg' using the format string 'fmt'.  Return the number of
 * fields scanned, or -1 if an input error is encounterd.  The
 * valid format specifiers are:
 *
 * %*X  Arbitrary sequence of hex bytes, where the number
 *      of bytes is returned by the first argument, and the
 *      data is returned into the second argument.  The data
 *      pointer is allocated by xmalloc() and must be later
 *      freed by xfree().
 *
 * %*s  An arbitrary string of characters possibly terminated
 *      by a comma, newline, or null character.  The string
 *      pointer is allocated by xmalloc() and must be later
 *      freed by xfree().
 *
 * %*b  Arbitrary sequence of bytes encoded using the remote
 *      protocol binary data encoding, where the number
 *      of bytes is returned in the first argument, and the
 *      data is returned into the second argument.  The data
 *      pointer is allocated by xmalloc() and must be later
 *      freed by xfree().
 *
 * %lx  Signed hex 64-bit word, leading zero bytes suppressed
 *      represented in big-endian form.  A leading '-'
 *      indicates a negative number.
 *
 * %lux Unsigned hex 64-bit word, leading zero bytes suppressed
 *      represented in big-endian form.
 *
 *	White space in the format string matches white space
 *	in the message string.
 *
 *	Any other character in the format string must exactly
 *	match the next character in the message string.
 *
 */
int
uda_rmt_vscan_msg (const char *msg, const char *fmt, va_list ap_arg)
{
  va_list ap;
  const char *fp, *cp;
  int ret;
  va_copy (ap, ap_arg);
  gdb_assert (fmt);
  for (fp = fmt, cp = msg, ret = 0; *fp; ++fp)
    {
      if (*fp == '%')
	{
	  ++fp;
	  if (*fp == '*')
	    {
	      ++fp;
	      if (*fp == 'b')
		{
		  size_t *l_ptr = va_arg (ap, size_t *);
		  char **b_ptr = va_arg (ap, char **);
		  const char *b1 = cp;
		  char *bp = 0;
		  size_t b_len;
                  b_len = uda_scan_binary_data (&cp);
		  if (b_len > 0)
		    {
		      size_t n_bytes;
		      bp = (char *) xmalloc (b_len);
		      n_bytes = (cp - b1);
		      (void) uda_decode_binary_data (bp, b1, n_bytes);
		    }
		  *l_ptr = b_len;
		  *b_ptr = bp;
		  ++ret;
		}
	      else if (*fp == 's')
		{
		  char **s_ptr = va_arg (ap, char **);
		  const char *s1 = cp;
		  char *str = 0;
		  size_t s_len;
		  /* scan until terminating ',' or '\n', as long as
		     these delimiters aren't escaped by preceding '\'.  */
		  for (s_len = 0; *cp && ((*cp != '\n' && *cp != ',')
			          || (cp > s1 && cp[-1] == '\\')); ++cp)
		    ++s_len;
		  if (s_len > 0)
		    {
		      str = (char *) xmalloc (s_len + 1);
		      memcpy (str, s1, s_len);
		      str[s_len] = '\0';
		    }
		  *s_ptr = str;
		  ++ret;
		}
	      else if (*fp == 'X')
		{
		  size_t *l_ptr = va_arg (ap, size_t *);
		  char **x_ptr = va_arg (ap, char **);
		  const char *x1 = cp;
		  size_t d_len = 0;
		  char *dp = 0;
		  size_t x_len;
		  for (x_len = 0; isxdigit (*cp); ++cp)
		    ++x_len;
		  if (x_len > 0)
		    {
		      d_len = (x_len + 1) / 2;
		      dp = (char *) xmalloc (d_len);
		      (void) uda_decode_hex_bytes (*x_ptr, x1, x_len);
		    }
		  *l_ptr = d_len;
		  *x_ptr = dp;
		  ++ret;
		}
	      else
		goto fmt_error;
	    }
	  else
	    {
	      int l_flag = 0;
	      int u_flag = 0;
	      int sign = 1;
	      if (*fp != 'l')
		goto fmt_error;
	      ++fp;
	      l_flag = 1;
	      if (*fp == 'u')
		{
		  ++fp;
		  u_flag = 1;
		}
	      if (*fp == 'x')
		{
	          ULONGEST val;
	          const char *x1;
		  size_t x_len;
		  if (*cp == '+' || *cp == '-')
		    {
		      if (u_flag)
			goto fmt_error;
		      if (*cp == '-')
			sign = -1;
		      ++cp;
		    }
		  for (x1 = cp, x_len = 0; isxdigit (*cp); ++cp)
		    ++x_len;
		  val = uda_decode_hex_word (x1, x_len);
		  if (u_flag)
		    {
	              ULONGEST *uval = va_arg (ap, ULONGEST *);
		      *uval = val;
		    }
		  else
		    {
	              LONGEST *ival = va_arg (ap, LONGEST *);
		      *ival = sign * (LONGEST) val;
		    }
		  ++ret;
		}
	      else
		goto fmt_error;
	    }
	}
      else if (isspace (*fp))
	{
	  while (isspace (fp[1]))
	    ++fp;
	  if (*cp)
	    {
	      if (!isspace(*cp))
		return -1;
	      while (isspace (*cp))
		++cp;
	    }
	}
      else
	{
	  if (*cp)
	    {
	      if (*cp != *fp)
		return -1;
	      ++cp;
	    }
	}
    }
  va_end (ap);
  /* We should consume the entire input string. */
  gdb_assert (!*cp);
  return ret;
fmt_error:
  fprintf (stderr, "uda_rmt_vscan_msg: bad format string: %s\n", fmt);
  abort ();
}

/* Convert an error code from the debugger into an error message 
 * (this cannot fail since it returns a string including the error
 * number if it is unknown */

const char *
uda_db_error_string (int error_code)
{
  const char *msg;
  static char mbuf[21];
  switch (error_code)
    {
    case uda_unimplemented:
      msg = "UDA: unimplemented operation"; break;
    case uda_ok:
      msg = "UDA: OK"; break;
    case uda_bad_assistant:
      msg = "UDA: bad assistant"; break;
    case uda_bad_job:
      msg = "UDA: bad uda_job"; break;
    case uda_bad_num_threads:
      msg = "UDA: bad num threads"; break;
    case uda_bad_thread_index:
      msg = "UDA: bad thread index"; break;
    case uda_no_information:
      msg = "UDA: no information"; break;
    case uda_no_symbol:
      msg = "UDA: no symbol"; break;
    case uda_num_threads_already_set:
      msg = "UDA: num threads already set"; break;
    case uda_read_failed:
      msg = "UDA: read failed"; break;
    case uda_write_failed:
      msg = "UDA: write failed"; break;
    case uda_relocation_failed:
      msg = "UDA: relocation failed"; break;
    case uda_target_sizes_already_set:
      msg = "UDA: target sizes already set"; break;
    default:
      sprintf (mbuf, "UDA: error %d",  error_code);
      msg = mbuf;
      break;
    }
  return msg;
}

/* Copy 'bytes' to 'be_bytes', reversing the order
   of the bytes copied if executing in a little-endian
   environment.  Convert in place is supported.  */

static void
uda_rmt_be_cvt (gdb_byte *be_bytes,
		const gdb_byte *bytes,
		const size_t n_bytes)
{
  if (uda_rmt_is_big_end)
    {
      if (be_bytes != bytes)
	memcpy (be_bytes, bytes, n_bytes);
    }
  else
    uda_rmt_swap_bytes (be_bytes, bytes, n_bytes);
}

void
uda_rmt_swap_bytes (gdb_byte *dest,
	            const gdb_byte *src,
	            const size_t n_bytes)
{
  int i, k;
  const int mid = (n_bytes + 1) / 2;
  for (i = 0, k = n_bytes - 1; i < mid; ++i, --k)
    {
      gdb_byte t = src[i];
      dest[i] = src[k];
      dest[k] = t;
    }
}

void
uda_rmt_init (FILE *rmt_in, FILE *rmt_out,
	      const uda_rmt_cmd_fp_t rmt_cmd_exec)
{
  volatile int i = 1;
  volatile char *p = (volatile char *) &i;
  uda_rmt_is_big_end = (*p == 0);
  uda_rmt_in = rmt_in;
  uda_rmt_out = rmt_out;
  uda_rmt_cmd_exec = rmt_cmd_exec;
}

static void
show_debug_uda (struct ui_file *file, int from_tty,
		struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("UDA debugging is %s.\n"), value);
}

void
_initialize_uda_rmt_utils (void)
{
  add_setshow_zinteger_cmd ("uda", class_maintenance, &debug_uda, _("\
Set UPC Debugger Assistant (UDA) protocol debugging."), _("\
Show UPC Debugger Assistant (UDA) protocol debugging."), _("\
When non-zero, UPC Debugger Assistant (UDA) protocol debugging is enabled."),
			    NULL,
			    show_debug_uda,
			    &setdebuglist, &showdebuglist);
}
