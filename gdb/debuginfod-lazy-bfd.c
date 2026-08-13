/* A lazily downloaded debug info file.

   Copyright (C) 2026 Free Software Foundation, Inc.

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

#include "debuginfod-lazy-bfd.h"
#include "gdbcore.h"
#include "extract-store-integer.h"
#include "build-id.h"
#include "debuginfod-support.h"

#include <elf.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

static
void apb_debug (const char *format, ...)
{
  if (getenv ("APB_DEBUG") == nullptr)
    return;

  va_list ap;
  std::string msg ("APB: ");

  va_start (ap, format);
  string_vappendf (msg, format, ap);
  va_end (ap);

  msg += "\n";

  fputs (msg.c_str (), stderr);
}

struct region
{
  off_t offset () const
  { return m_offset; }

  size_t length () const
  { return m_length; }

  int read (void *buffer, file_ptr nbytes,
	    file_ptr offset) const;

  /* Return true if the continuous region starting at OFFSET and
     extending for LENGTH bytes is within THIS region.  */
  bool in_region (off_t offset, size_t length) const
  {
    /* First byte we wish to read.  */
    off_t start = offset;

    /* First byte AFTER the region we wish to read.  */
    off_t end = offset + length;

    return (start >= m_offset && start <= (m_offset + m_length)
	    && end >= m_offset && end <= (m_offset + m_length));
  }

  region (off_t offset, size_t length, const void *addr)
    : m_offset (offset),
      m_length (length),
      m_addr (addr)
  { /* Nothing.  */ }

  region (off_t offset, size_t length, gdb::byte_vector &&content)
    : m_offset (offset),
      m_length (length),
      m_content (std::move (content))
  {
    m_addr = m_content.data ();
  }

private:
  /* The offset into the full debug info file.  */
  off_t m_offset;

  /* The length of this region within the full debug info file.  */
  size_t m_length;

  /* The location in memory of this region.  */
  const void *m_addr;

  /* .... */
  gdb::byte_vector m_content;
};

struct section
{
  section (std::string name, off_t offset, size_t length)
    : m_name (std::move (name)),
      m_offset (offset),
      m_length (length)
  { /* Nothing.  */ }

  off_t offset () const
  { return m_offset; }

  size_t length () const
  { return m_length; }

  const std::string &name () const
  { return m_name; }

  bool in_section (off_t offset, size_t length) const
  {
    /* First byte we wish to read.  */
    off_t start = offset;

    /* First byte AFTER the region we wish to read.  */
    off_t end = offset + length;

    return (start >= m_offset && start <= (m_offset + m_length)
	    && end >= m_offset && end <= (m_offset + m_length));
  }

private:
  /* Name of the section.  */
  std::string m_name;

  /* Offset in file for start of section.  */
  off_t m_offset;

  /* Length of the section in the file.  */
  size_t m_length;
};

/* See declaration above.  */

int
region::read (void *buffer, file_ptr nbytes, file_ptr offset) const
{
  /* The offset within this region.  */
  offset -= this->offset ();

  gdb_assert (offset + nbytes <= m_offset + m_length);

  memcpy (buffer, (void *) (((uintptr_t) m_addr) + offset), nbytes);
  return nbytes;
}

/* ... */

template<typename T>
T
read_elf (gdb_byte *&buffer, bfd_endian byte_order)
{
  T result = static_cast<T> (extract_unsigned_integer (buffer, sizeof (T), byte_order));
  buffer += sizeof (T);
  return result;
}

template<typename T>
void
write_elf (gdb_byte *&buffer, bfd_endian byte_order, T value)
{
  store_unsigned_integer (buffer, sizeof (T), byte_order, value);
  buffer += sizeof (T);
}

struct gdb_debuginfod_deferred_download : public gdb_bfd_iovec_base
{
  gdb_debuginfod_deferred_download (scoped_fd &&fd,
				    const bfd_build_id *build_id,
				    objfile *objfile);
  
  ~gdb_debuginfod_deferred_download ()
  {
    /* Nothing.  */
  }

  file_ptr read (bfd *abfd, void *buffer, file_ptr nbytes,
		 file_ptr offset) override;

  int stat (struct bfd *abfd, struct stat *sb) override;
  
private:

  using section_header_fn
  = gdb::function_view<void (gdb_byte *address,
			     const std::string &name,
			     ULONGEST sh_type, ULONGEST sh_offset,
			     ULONGEST sh_size)>;

  void foreach_section_header (gdb_byte *headers, bfd_endian byte_order,
			       size_t shentsize, int shnum,
			       gdb_byte *shstrtab_addr, size_t shstrtab_size,
			       section_header_fn cb);

  void make_section_nobits (gdb_byte *address,
			    bfd_endian byte_order,
			    size_t shentsize);

  /* An open file descriptor for the skeleton debug information.  */
  scoped_fd m_fd;

  /* ... */
  const bfd_build_id *m_build_id;

  /* ... */
  objfile *m_objfile;

  /* Size of the complete debuginfo file.  This is not the size of the
     skeleton file, but the size of the full debug info file the
     skeleton represents.  */
  size_t m_file_size;

  /* ... */
  std::vector<region> m_regions;

  /* ... */
  void *m_data = nullptr;

  /* ... */
  std::vector<section> m_sections;
};

/* ... */
static unsigned char
read_byte (gdb_byte *buffer)
{
  unsigned char b;
  memcpy (&b, buffer, sizeof (b));
  return b;
}

void
gdb_debuginfod_deferred_download::make_section_nobits (gdb_byte *address,
						       bfd_endian byte_order,
						       size_t shentsize)
{
  ULONGEST sh_type = SHT_NOBITS;

  if (shentsize == sizeof (Elf32_Shdr))
    {
      /* Skip over sh_name field, write back to sh_type.  */
      gdb_byte *tmp = address + sizeof (Elf32_Word);
      write_elf<Elf32_Word> (tmp, byte_order, sh_type);
    }
  else
    {
      gdb_assert (shentsize == sizeof (Elf64_Shdr));
      /* Skip over sh_name field, write back to sh_type.  */
      gdb_byte *tmp = address + sizeof (Elf64_Word);
      write_elf<Elf64_Word> (tmp, byte_order, sh_type);
    }
}

void
gdb_debuginfod_deferred_download::foreach_section_header
  (gdb_byte *headers, bfd_endian byte_order,
   size_t shentsize, int shnum,
   gdb_byte *shstrtab_addr, size_t shstrtab_size,
   section_header_fn cb)
{
  for (int i = 0; i < shnum; ++i)
    {
      gdb_byte *shaddr = headers + (i * shentsize);
      gdb_byte *ptr = shaddr;

      ULONGEST sh_name, sh_type, sh_offset, sh_size;
      if (shentsize == sizeof (Elf32_Shdr))
	{
	  sh_name = read_elf<Elf32_Word> (ptr, byte_order);
	  sh_type = read_elf<Elf32_Word> (ptr, byte_order);
	  ptr += sizeof (Elf32_Word);	/* sh_flags */
	  ptr += sizeof (Elf32_Addr);	/* sh_addr */
	  sh_offset = read_elf<Elf32_Off> (ptr, byte_order);
	  sh_size = read_elf<Elf32_Word> (ptr, byte_order);
	}
      else
	{
	  gdb_assert (shentsize == sizeof (Elf64_Shdr));
	  sh_name = read_elf<Elf64_Word> (ptr, byte_order);
	  sh_type = read_elf<Elf64_Word> (ptr, byte_order);
	  ptr += sizeof (Elf64_Xword);	/* sh_flags */
	  ptr += sizeof (Elf64_Addr);	/* sh_addr */
	  sh_offset = read_elf<Elf64_Off> (ptr, byte_order);
	  sh_size = read_elf<Elf32_Xword> (ptr, byte_order);
	}

      if (sh_name >= shstrtab_size)
	error (_("section name index outside string table"));

      /* Get the name from the string table.  */
      /* ... */
      std::string name;
      for (; sh_name < shstrtab_size; ++sh_name)
	{
	  gdb_byte c = read_byte (shstrtab_addr + sh_name);
	  if (c == '\0')
	    break;
	  name.push_back (c);
	}

      cb (shaddr, name, sh_type, sh_offset, sh_size);
    }
}

gdb_debuginfod_deferred_download::gdb_debuginfod_deferred_download
  (scoped_fd &&fd, const bfd_build_id *build_id, objfile *objfile)
    : m_fd (std::move (fd)),
      m_build_id (build_id),
      m_objfile (objfile)
{
  off_t loc = ::lseek (m_fd.get (), 0, SEEK_END);
  if (loc == (off_t) -1)
    error (_("failed to seek to end of skeleton file"));

  m_data = mmap (NULL, loc, PROT_READ | PROT_WRITE, MAP_PRIVATE, m_fd.get (), 0);
  if (m_data == MAP_FAILED)
    error (_("failed to map skeleton into memory"));

  gdb_byte *ptr = static_cast<gdb_byte *> (m_data);

  /* Read the one byte version number.  */
  unsigned char version = read_byte (ptr);
  if (version != 1)
    error (_("unknown version number: %d"), version);
  ++ptr;

  /* Read the ULEB128 encoded file size.  */
  size_t file_size = 0;
  int shift_distance = 0;
  unsigned char b;
  do
    {
      /* Check for overflow now rather than at the end of the loop.
	 This avoids false error in the case where we exactly fill
	 FILE_SIZE.  */
      if (shift_distance >= sizeof (file_size) * HOST_CHAR_BIT)
	error (_("uleb128 file size is too large"));

      b = read_byte (ptr);
      ++ptr;

      file_size |= ((b & 0x7f) << shift_distance);
      shift_distance += 7;
    }
  while ((b & 0x80) == 0x80);

  /* Location of the ELF header.  */
  gdb_byte *ehdr_addr = ptr;

  /* Parse the first few bytes from the ELF header.  */
  unsigned char e_ident[EI_NIDENT];
  memcpy (e_ident, ptr, sizeof (e_ident));
  if (e_ident[0] != ELFMAG0
      || e_ident[1] != ELFMAG1
      || e_ident[2] != ELFMAG2
      || e_ident[3] != ELFMAG3)
    error (_("invalid bytes found in ELF header"));
  if (e_ident[EI_VERSION] != EV_CURRENT)
    error (_("unsupported ELF version: %d"), e_ident[EI_VERSION]);
  ptr += sizeof (e_ident);

  bfd_endian byte_order;
  switch (e_ident[EI_DATA])
    {
    case ELFDATA2LSB:
      byte_order = BFD_ENDIAN_LITTLE;
      break;

    case ELFDATA2MSB:
      byte_order = BFD_ENDIAN_BIG;
      break;

    default:
      error (_("unknown elf data type: %d"), e_ident[EI_DATA]);
    }

  size_t ehdr_len;
  Elf64_Ehdr ehdr;
  switch (e_ident[EI_CLASS])
    {
    case ELFCLASS32:
      ehdr_len = sizeof (Elf32_Ehdr);
      ehdr.e_type = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_machine = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_version = read_elf<Elf32_Word> (ptr, byte_order);
      ehdr.e_entry = read_elf<Elf32_Addr> (ptr, byte_order);
      ehdr.e_phoff = read_elf<Elf32_Off> (ptr, byte_order);
      ehdr.e_shoff = read_elf<Elf32_Off> (ptr, byte_order);
      ehdr.e_flags = read_elf<Elf32_Word> (ptr, byte_order);
      ehdr.e_ehsize = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_phentsize = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_phnum = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_shentsize = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_shnum = read_elf<Elf32_Half> (ptr, byte_order);
      ehdr.e_shstrndx = read_elf<Elf32_Half> (ptr, byte_order);
      break;

    case ELFCLASS64:
      ehdr_len = sizeof (Elf64_Ehdr);
      ehdr.e_type = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_machine = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_version = read_elf<Elf64_Word> (ptr, byte_order);
      ehdr.e_entry = read_elf<Elf64_Addr> (ptr, byte_order);
      ehdr.e_phoff = read_elf<Elf64_Off> (ptr, byte_order);
      ehdr.e_shoff = read_elf<Elf64_Off> (ptr, byte_order);
      ehdr.e_flags = read_elf<Elf64_Word> (ptr, byte_order);
      ehdr.e_ehsize = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_phentsize = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_phnum = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_shentsize = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_shnum = read_elf<Elf64_Half> (ptr, byte_order);
      ehdr.e_shstrndx = read_elf<Elf64_Half> (ptr, byte_order);
      break;

    default:
      error (_("unknown elf class: %d"), e_ident[EI_CLASS]);
    }

  if (ehdr.e_shentsize != sizeof (Elf32_Shdr)
      && ehdr.e_shentsize != sizeof (Elf64_Shdr))
    error (_("unexpected section header entry size"));


  /* Region for the ELF header.  */
  m_regions.emplace_back (0, ehdr_len, ehdr_addr);

  /* Region for the program headers.  */
  gdb_byte *phdr_addr = ehdr_addr + ehdr_len;
  size_t phsize = ehdr.e_phentsize * ehdr.e_phnum;
  m_regions.emplace_back (ehdr.e_phoff, phsize, phdr_addr);

  /* Region for the section headers.  */
  gdb_byte *shdr_addr = phdr_addr + phsize;
  size_t shsize = ehdr.e_shentsize * ehdr.e_shnum;
  m_regions.emplace_back (ehdr.e_shoff, shsize, shdr_addr);

  /* Region for the string table.  This requires us to parse the section table.  */
  gdb_byte *shstrtab_shdr_addr = shdr_addr + ehdr.e_shstrndx * ehdr.e_shentsize;
  apb_debug ("section string table is index %d", ehdr.e_shstrndx);

  Elf64_Shdr shstrtab_shdr;
  if (ehdr.e_shentsize == sizeof (Elf32_Shdr))
    {
      shstrtab_shdr.sh_name = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_type = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_flags = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_addr = read_elf<Elf32_Addr> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_offset = read_elf<Elf32_Off> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_size = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_link = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_info = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_addralign = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_entsize = read_elf<Elf32_Word> (shstrtab_shdr_addr, byte_order);
    }
  else
    {
      gdb_assert (ehdr.e_shentsize == sizeof (Elf64_Shdr));
      shstrtab_shdr.sh_name = read_elf<Elf64_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_type = read_elf<Elf64_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_flags = read_elf<Elf64_Xword> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_addr = read_elf<Elf64_Addr> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_offset = read_elf<Elf64_Off> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_size = read_elf<Elf64_Xword> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_link = read_elf<Elf64_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_info = read_elf<Elf64_Word> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_addralign = read_elf<Elf64_Xword> (shstrtab_shdr_addr, byte_order);
      shstrtab_shdr.sh_entsize = read_elf<Elf64_Xword> (shstrtab_shdr_addr, byte_order);
    }

  gdb_byte *shstrtab_addr = shdr_addr + shsize;
  m_regions.emplace_back (shstrtab_shdr.sh_offset, shstrtab_shdr.sh_size,
			  shstrtab_addr);

  foreach_section_header (shdr_addr, byte_order,
			  ehdr.e_shentsize, ehdr.e_shnum,
			  shstrtab_addr, shstrtab_shdr.sh_size,
			  [&] (gdb_byte *address, const std::string &name,
			       ULONGEST sh_type, ULONGEST sh_offset,
			       ULONGEST sh_size) -> void
  {
    if (sh_type != SHT_NOBITS)
      m_sections.emplace_back (name, sh_offset, sh_size);

    apb_debug ("section %s at offset %s, size %s",
	     name.c_str (), phex_nz (sh_offset), phex_nz (sh_size));

    if (name == ".note.gnu.build-id")
      {
	apb_debug ("build_id->size = %ld", build_id->size);

	/* The build-id note section will be 12 bytes for the standard
	   note header, 4 bytes for the "GNU" name string, and 20
	   bytes for the build-id payload.  */
	if (build_id->size != 20)
	  error (_("build-id has an unexpected size"));

	gdb::byte_vector buf (36);
	store_unsigned_integer (&buf[0], sizeof (uint32_t), byte_order, 4);
	store_unsigned_integer (&buf[4], sizeof (uint32_t), byte_order, 16);
	store_unsigned_integer (&buf[8], sizeof (uint32_t), byte_order,
				NT_GNU_BUILD_ID);
	buf[12] = 'G';
	buf[13] = 'N';
	buf[14] = 'U';
	buf[15] = '\0';
	memcpy (&buf[16], build_id->data, build_id->size);

	/* Need to provide this section from the build-id.  */
	m_regions.emplace_back (sh_offset, sh_size, std::move (buf));
      }
    else if (startswith (name, ".debug_"))
      {
	/* Need to fetch the whole debug info file.  */
	apb_debug ("must arrange for %s to trigger full download", name.c_str ());
      }
    else if (name == ".gdb_index")
      {
	/* Need to fetch just this section and use it for the read.  */
	apb_debug ("must arrange for .gdb_index to be downloaded");
      }
    else if (sh_type == SHT_NOTE || sh_type == SHT_SYMTAB)
      {
	apb_debug ("marking section %s as no-bits", name.c_str ());

	/* Do not mark the section header string table as NOBITS.  */
	if (address != shstrtab_shdr_addr)
	  make_section_nobits (address, byte_order, ehdr.e_shentsize);
      }
  });

  apb_debug ("got a file with version number: %d", version);
  apb_debug ("and a file size of %lld bytes",
	   ((unsigned long long) file_size));
  apb_debug ("and %ld regions", m_regions.size ());
}

file_ptr gdb_debuginfod_deferred_download::read (bfd *abfd, void *buffer,
						 file_ptr nbytes,
						 file_ptr offset)
{
  try
  {
  for (const region &r : m_regions)
    {
      if (!r.in_region (offset, nbytes))
	continue;

      /* This read is entirely within this region.  */
      return r.read (buffer, nbytes, offset);
    }

  const section *in_section = nullptr;
  for (const section &s : m_sections)
    {
      if (!s.in_section (offset, nbytes))
	continue;

      in_section = &s;
      break;
    }

  apb_debug ("gdb_debuginfod_deferred_download::read");
  apb_debug ("     bfd = %s", bfd_get_filename (abfd));
  apb_debug ("     offset = %s", phex_nz (offset));
  apb_debug ("     length = %s", phex_nz (nbytes));

  if (in_section != nullptr)
    {
      apb_debug ("     section = %s", in_section->name ().c_str ());

      if (in_section->offset () == offset && in_section->length () >= nbytes
	  && nbytes == 12
	  && startswith (in_section->name (), ".debug_"))
	{
	  apb_debug ("!!!! Faking debug section header.");
	  /* This could be a read of a compression header within a
	     section?  If it is we return zeros for now.  This will
	     make BFD think the section is not compressed, but we fix
	     this later when we download the actual section.  */
	  memset (buffer, 0, nbytes);
	  return nbytes;
	}

      /* NOTE: I tried adding ".debug_line" support here, but GDB will
	 first try to read additional information from .debug_info,
	 .debug_abbrev, .debug_str, and .debug_line_str, before
	 reading the .debug_line section.  */
      if ((in_section->name () == ".gdb_index"
	   || in_section->name () == ".debug_gdb_scripts"
	   || in_section->name () == ".gnu_debugaltlink")
	  && offset == in_section->offset ()
	  && nbytes == in_section->length ())
	{
	  apb_debug ("!!!! Downloading just this one section.");

	  /* Read of entire .gdb_index section.  Download the section
	     and serve the content from that section.

	     TODO: We should also handle the DWARF5 index sections
	     here too.  */
	  const char *filename = bfd_get_filename (m_objfile->obfd.get ());
	  gdb::unique_xmalloc_ptr<char> destname;
	  scoped_fd section_fd = debuginfod_section_query (m_build_id->data,
							   m_build_id->size,
							   filename,
							   in_section->name ().c_str (),
							   &destname);

	  /* Confirm that the downloaded file is the expected size.
	     If not return EIO to indicate an error.  */
	  struct stat statbuf;
	  if (fstat (section_fd.get (), &statbuf) != 0)
	    error (_("failed to fstat section file descriptor"));
	  if (statbuf.st_size != nbytes)
	    error (_("return section file is not the required size"));

	  if (::lseek (section_fd.get (), 0, SEEK_SET) != (off_t) 0)
	    error (_("failed to seek in downloaded section"));

	  /* Copy content from SECTION_FD into the outbound BUFFER.  */
	  if (::read (section_fd.get (), buffer, nbytes) != nbytes)
	    error (_("failed to read from section file into read buffer"));
	  return nbytes;
	}

      if (startswith (in_section->name (), ".debug_") || true)
	{
	  apb_debug ("!!!! Downloading the full debug information");

	  /* Download the wholte file.  */
	  const char *filename = bfd_get_filename (m_objfile->obfd.get ());
	  gdb::unique_xmalloc_ptr<char> destname;
	  scoped_fd fd = debuginfod_debuginfo_query (m_build_id->data,
						     m_build_id->size,
						     filename,
						     &destname);

	  if (::lseek (fd.get (), offset, SEEK_SET) != (off_t) offset)
	    error (_("failed to seek in full debuginfo file"));

	  if (::read (fd.get (), buffer, nbytes) != nbytes)
	    error (_("failed to read from full debuginfo file"));

	  return nbytes;
	}
    }

  apb_debug ("!!!! This read is not in any region");

  return 0;
  }
  catch (const gdb_exception_error &e)
    {
      apb_debug ("EXCEPTION: %s", e.what ());
      throw;
    }
}

int gdb_debuginfod_deferred_download::stat (struct bfd *abfd, struct stat *sb)
{
  memset (sb, 0, sizeof (*sb));
  sb->st_size = m_file_size;
  return 0;
}

gdb_bfd_ref_ptr
debuginfod_open_deferred_download_skeleton (objfile *objfile, scoped_fd &&fd,
					    const char *filename)
{
  /* TODO: The lifetime of this pointer is bounded by the life time of
     OBJFILE.  If we hold the pointer within the
     gdb_debuginfod_deferred_download object could it outlive
     OBJFILE?  */
  const struct bfd_build_id *build_id
    = build_id_bfd_get (objfile->obfd.get ());

  auto open = [&] (bfd *nbfd) -> gdb_debuginfod_deferred_download *
  {
    try
      {
	/* TODO: I don't think this is correct.  BFD will 'free' the
	   object returned here, which isn't going to call the
	   destructor.  Need to figure out what the correct choice
	   is.  */
	return new gdb_debuginfod_deferred_download (std::move (fd), build_id, objfile);
      }
    catch (gdb_exception_error &e)
      {
	warning (_("unable to open deferred download debug skeleton: %s"),
		 e.what ());
	return nullptr;
      }
  };

  gdb_bfd_ref_ptr abfd = gdb_bfd_openr_iovec (filename, gnutarget, open);
  if (abfd == nullptr)
    return nullptr;

  if (!gdb_bfd_check_format (abfd.get (), bfd_object))
    {
      warning (_("Cannot parse debug info skeleton; not a BFD object"));
      return nullptr;
    }
  
  return abfd;
}
