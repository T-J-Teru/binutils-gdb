/* This testcase is part of GDB, the GNU debugger.

   Copyright 2026 Free Software Foundation, Inc.

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

/* Create a .gdb_index section that is too small to hold a valid header.
   The size of the section, in bytes, is given by the INDEX_SIZE macro
   which the exp script sets when this file is assembled.

   A valid version 9 header is 28 bytes: a 4-byte version word followed by
   six 4-byte offsets.  Any section smaller than this is invalid and GDB
   should reject it rather than reading beyond the end of the section.

   The .gdb_index format is always little-endian, so we build the section
   byte by byte rather than with directives like .4byte, whose encoding
   would depend on the host's endianness.  The first byte is 0x09, the low
   byte of a little-endian version 9 word; any remaining bytes are zero.
   For sizes of 4 or more this yields a valid version word (0x09 0x00 0x00
   0x00) so that GDB gets past its version check and on to the header size
   check, with the trailing zeros standing in for a truncated set of header
   offsets.  For sizes below 4 there is not even room for the full version
   word.  */

	.section .gdb_index
#if INDEX_SIZE > 0
	/* Low byte of a little-endian version 9 word.  */
	.byte 0x09
# if INDEX_SIZE > 1
	/* Rest of the truncated header.  GDB checks the header size
	   before checking that the header contents are valid, so
	   filling with zeros, which is invalid, doesn't really
	   matter.  */
	.fill (INDEX_SIZE - 1), 1, 0
# endif
#endif

	.section	.note.GNU-stack,"",%progbits
