/* ELF specific corefile related functionality.

   Copyright (C) 2024 Free Software Foundation, Inc.

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

#include "elf-corelow.h"
#include "inferior.h"
#include "auxv.h"
#include "gdbcore.h"
#include "elf/common.h"
#include "bfd/elf-bfd.h"

/* Try to extract the inferior arguments, environment, and executable name
   from CBFD.  */

static core_file_exec_context
elf_corefile_parse_exec_context_1 (struct gdbarch *gdbarch, bfd *cbfd)
{
  gdb_assert (gdbarch != nullptr);

  /* If there's no core file loaded then we're done.  */
  if (cbfd == nullptr)
    return {};

  /* This function (currently) assumes the stack grows down.  If this is
     not the case then this function isn't going to help.  */
  if (!gdbarch_stack_grows_down (gdbarch))
    return {};

  int ptr_bytes = gdbarch_ptr_bit (gdbarch) / TARGET_CHAR_BIT;

  /* Find the .auxv section in the core file. The BFD library creates this
     for us from the AUXV note when the BFD is opened.  If the section
     can't be found then there's nothing more we can do.  */
  struct bfd_section * section = bfd_get_section_by_name (cbfd, ".auxv");
  if (section == nullptr)
    return {};

  /* Grab the contents of the .auxv section.  If we can't get the contents
     then there's nothing more we can do.  */
  bfd_size_type size = bfd_section_size (section);
  if (bfd_section_size_insane (cbfd, section))
    return {};
  gdb::byte_vector contents (size);
  if (!bfd_get_section_contents (cbfd, section, contents.data (), 0, size))
    return {};

  /* Parse the .auxv section looking for the AT_EXECFN attribute.  The
     value of this attribute is a pointer to a string, the string is the
     executable command.  Additionally, this string is placed at the top of
     the program stack, and so will be in the same PT_LOAD segment as the
     argv and envp arrays.  We can use this to try and locate these arrays.
     If we can't find the AT_EXECFN attribute then we're not going to be
     able to do anything else here.  */
  CORE_ADDR execfn_string_addr;
  if (target_auxv_search (contents, current_inferior ()->top_target (),
			  gdbarch, AT_EXECFN, &execfn_string_addr) != 1)
    return {};

  /* Read in the program headers from CBFD.  If we can't do this for any
     reason then just give up.  */
  long phdrs_size = bfd_get_elf_phdr_upper_bound (cbfd);
  if (phdrs_size == -1)
    return {};
  gdb::unique_xmalloc_ptr<Elf_Internal_Phdr>
    phdrs ((Elf_Internal_Phdr *) xmalloc (phdrs_size));
  int num_phdrs = bfd_get_elf_phdrs (cbfd, phdrs.get ());
  if (num_phdrs == -1)
    return {};

  /* Now scan through the headers looking for the one which contains the
     address held in EXECFN_STRING_ADDR, this is the address of the
     executable command pointed too by the AT_EXECFN auxv entry.  */
  Elf_Internal_Phdr *hdr = nullptr;
  for (int i = 0; i < num_phdrs; i++)
    {
      /* The program header that contains the address EXECFN_STRING_ADDR
	 should be one where all content is contained within CBFD, hence
	 the check that the file size matches the memory size.  */
      if (phdrs.get ()[i].p_type == PT_LOAD
	  && phdrs.get ()[i].p_vaddr <= execfn_string_addr
	  && (phdrs.get ()[i].p_vaddr
	      + phdrs.get ()[i].p_memsz) > execfn_string_addr
	  && phdrs.get ()[i].p_memsz == phdrs.get ()[i].p_filesz)
	{
	  hdr = &phdrs.get ()[i];
	  break;
	}
    }

  /* If we failed to find a suitable program header then give up.  */
  if (hdr == nullptr)
    return {};

  /* As we assume the stack grows down (see early check in this function)
     we know that the information we are looking for sits somewhere between
     EXECFN_STRING_ADDR and the segments virtual address.  These define
     the HIGH and LOW addresses between which we are going to search.  */
  CORE_ADDR low = hdr->p_vaddr;
  CORE_ADDR high = execfn_string_addr;

  /* This PTR is going to be the address we are currently accessing.  */
  CORE_ADDR ptr = align_down (high, ptr_bytes);

  /* Setup DEREF a helper function which loads a value from an address.
     The returned value is always placed into a uint64_t, even if we only
     load 4-bytes, this allows the code below to be pretty generic.  All
     the values we're dealing with are unsigned, so this should be OK.   */
  enum bfd_endian byte_order = gdbarch_byte_order (gdbarch);
  gdb::function_view<uint64_t (CORE_ADDR)> deref
    = [=] (CORE_ADDR p) -> uint64_t
    {
      ULONGEST value = read_memory_unsigned_integer (p, ptr_bytes, byte_order);
      return (uint64_t) value;
    };

  /* Now search down through memory looking for a PTR_BYTES sized object
     which contains the value EXECFN_STRING_ADDR.  The hope is that this
     will be the AT_EXECFN entry in the auxv table.  There is no guarantee
     that we'll find the auxv table this way, but we will do our best to
     validate that what we find is the auxv table, see below.  */
  while (ptr > low)
    {
      if (deref (ptr) == execfn_string_addr
	  && (ptr - ptr_bytes) > low
	  && deref (ptr - ptr_bytes) == AT_EXECFN)
	break;

      ptr -= ptr_bytes;
    }

  /* If we reached the lower bound then we failed -- bail out.  */
  if (ptr <= low)
    return {};

  /* Assuming that we are looking at a value field in the auxv table, move
     forward PTR_BYTES bytes so we are now looking at the next key field in
     the auxv table, then scan forward until we find the null entry which
     will be the last entry in the auxv table.  */
  ptr += ptr_bytes;
  while ((ptr + (2 * ptr_bytes)) < high
	 && (deref (ptr) != 0 || deref (ptr + ptr_bytes) != 0))
    ptr += (2 * ptr_bytes);

  /* PTR now points to the null entry in the auxv table, or we think it
     does.  Now we want to find the start of the auxv table.  There's no
     in-memory pattern we can search for at the start of the table, but
     we can find the start based on the size of the .auxv section within
     the core file CBFD object.  In the actual core file the auxv is held
     in a note, but the bfd library makes this into a section for us.

     The addition of (2 * PTR_BYTES) here is because PTR is pointing at the
     null entry, but the null entry is also included in CONTENTS.  */
  ptr = ptr + (2 * ptr_bytes) - contents.size ();

  /* If we reached the lower bound then we failed -- bail out.  */
  if (ptr <= low)
    return {};

  /* PTR should now be pointing to the start of the auxv table mapped into
     the inferior memory.  As we got here using a heuristic then lets
     compare an auxv table sized block of inferior memory, if this matches
     then it's not a guarantee that we are in the right place, but it does
     make it more likely.  */
  gdb::byte_vector target_contents (size);
  if (target_read_memory (ptr, target_contents.data (), size) != 0)
    memory_error (TARGET_XFER_E_IO, ptr);
  if (memcmp (contents.data (), target_contents.data (), size) != 0)
    return {};

  /* We have reasonable confidence that PTR points to the start of the auxv
     table.  Below this should be the null terminated list of pointers to
     environment strings, and below that the null terminated list of
     pointers to arguments strings.  After that we should find the
     argument count.  First, check for the null at the end of the
     environment list.  */
  if (deref (ptr - ptr_bytes) != 0)
    return {};

  ptr -= (2 * ptr_bytes);
  while (ptr > low && deref (ptr) != 0)
    ptr -= ptr_bytes;

  /* If we reached the lower bound then we failed -- bail out.  */
  if (ptr <= low)
    return {};

  /* PTR is now pointing to the null entry at the end of the argument
     string pointer list.  We now want to scan backward to find the entire
     argument list.  There's no handy null marker that we can look for
     here, instead, as we scan backward we look for the argument count
     (argc) value which appears immediately before the argument list.

     Technically, we could have zero arguments, so the argument count would
     be zero, however, we don't support this case.  If we find a null entry
     in the argument list before we find the argument count then we just
     bail out.

     Start by moving to the last argument string pointer, we expect this
     to be non-null.  */
  ptr -= ptr_bytes;
  uint64_t argc = 0;
  while (ptr > low)
    {
      uint64_t val = deref (ptr);
      if (val == 0)
	return {};

      if (val == argc)
	break;

      argc++;
      ptr -= ptr_bytes;
    }

  /* If we reached the lower bound then we failed -- bail out.  */
  if (ptr <= low)
    return {};

  /* PTR is now pointing at the argument count value.  Move it forward
     so we're pointing at the first actual argument string pointer.  */
  ptr += ptr_bytes;

  /* We can now parse all of the argument strings.  */
  std::vector<gdb::unique_xmalloc_ptr<char>> arguments;

  /* Skip the first argument.  This is the executable command, but we'll
     load that separately later.  */
  ptr += ptr_bytes;

  uint64_t v;
  while ((v = deref (ptr)) != 0)
    {
      gdb::unique_xmalloc_ptr<char> str = target_read_string (v, INT_MAX);
      if (str == nullptr)
	return {};
      arguments.emplace_back (std::move (str));
      ptr += ptr_bytes;
    }

  /* Skip the null-pointer at the end of the argument list.  We will now
     be pointing at the first environment string.  */
  ptr += ptr_bytes;

  /* Parse the environment strings.  */
  std::vector<gdb::unique_xmalloc_ptr<char>> environment;
  while ((v = deref (ptr)) != 0)
    {
      gdb::unique_xmalloc_ptr<char> str = target_read_string (v, INT_MAX);
      if (str == nullptr)
	return {};
      environment.emplace_back (std::move (str));
      ptr += ptr_bytes;
    }

  gdb::unique_xmalloc_ptr<char> execfn
    = target_read_string (execfn_string_addr, INT_MAX);
  if (execfn == nullptr)
    return {};

  /* When the core-file was loaded GDB processed the file backed mappings
     (NT_FILES).  One of these should have been for the executable.  Now
     the AT_EXECFN string might not be an absolute path, but the path in
     NT_FILES will be absolute, though if AT_EXECFN is a symlink, then the
     NT_FILES entry will point to the actual file, not the symlink.

     So use the AT_ENTRY address to look for the NT_FILES entry which
     contains that address, this should be the executable.  We store this
     absolute filename, we might be able to use this later to auto-load the
     executable that matches the core file.  */
  gdb::unique_xmalloc_ptr<char> exec_filename;
  CORE_ADDR exec_entry_addr;
  if (target_auxv_search (contents, current_inferior ()->top_target (),
			  gdbarch, AT_ENTRY, &exec_entry_addr) == 1)
    {
      std::optional<core_target_mapped_file_info> info
	= core_target_find_mapped_file (nullptr, exec_entry_addr);
      if (info.has_value () && !info->filename ().empty ()
	  && IS_ABSOLUTE_PATH (info->filename ().c_str ()))
	exec_filename = make_unique_xstrdup (info->filename ().c_str ());
    }

  return core_file_exec_context (std::move (execfn),
				 std::move (exec_filename),
				 std::move (arguments),
				 std::move (environment));
}

/* See elf-corelow.h.  */

core_file_exec_context
elf_corefile_parse_exec_context (struct gdbarch *gdbarch, bfd *cbfd)
{
  /* Catch and discard memory errors.

     If the core file format is not as we expect then we can easily trigger
     a memory error while parsing the core file.  We don't want this to
     prevent the user from opening the core file; the information provided
     by this function is helpful, but not critical, debugging can continue
     without it.  Instead just give a warning and return an empty context
     object.  */
  try
    {
      return elf_corefile_parse_exec_context_1 (gdbarch, cbfd);
    }
  catch (const gdb_exception_error &ex)
    {
      if (ex.error == MEMORY_ERROR)
	{
	  warning
	    (_("failed to parse execution context from corefile: %s"),
	     ex.message->c_str ());
	  return {};
	}
      else
	throw;
    }
}

