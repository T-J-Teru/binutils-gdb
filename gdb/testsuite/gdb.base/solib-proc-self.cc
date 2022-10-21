/* This testcase is part of GDB, the GNU debugger.

   Copyright 2022 Free Software Foundation, Inc.

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

#include <sys/mman.h>
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <unistd.h>

#ifdef __WIN32__
#include <windows.h>
#define dlopen(name, mode) LoadLibrary (name)
#define dlclose(handle) FreeLibrary (handle)
#define dlerror() "an error occurred"
#else
#include <dlfcn.h>
#endif

/* Combine PREFIX and NUM into a single string, joined with a '/', and
   return the string.  */

std::string
make_library_path (const char *prefix, int num)
{
  std::stringstream stream;
  stream << prefix << "/" << num;
  return stream.str ();
}

/* Call dlopen on the library pointed to by FILENAME.  */

void
open_library (const std::string &filename)
{
  /* Call dlopen on FILENAME.  */
  void *handle = dlopen (filename.c_str (), RTLD_LAZY);
  if (handle == nullptr)
    abort ();

  /* It worked.  */
  dlclose (handle);
}

int
main ()
{
  /* Read the shared libraries contents into a buffer.  */
  std::ifstream read_so_file = std::ifstream (SHLIB_NAME);
  read_so_file.seekg (0, std::ios::end);
  std::streamsize size = read_so_file.tellg ();
  read_so_file.seekg (0, std::ios::beg);
  std::vector<char> buffer (size);
  if (!read_so_file.read (buffer.data (), size))
    abort ();

  /* Create a memory mapped file, then write the shared library to that
     new memory mapped file.  */
  int mem_fd = memfd_create ("test", 0);
  write (mem_fd, buffer.data (), buffer.size ());

  /* Generate a canonical /proc/self/fd/[num] path for the memory mapped
     file, and call dlopen on it.  */
  std::string filename
    = make_library_path ("/proc/self/fd", mem_fd);	/* break-here */
  open_library (filename);

  /* Now generate a new, non-canonical filename, and call dlopen on it.  */
  filename = make_library_path ("/proc/../proc/self/fd", mem_fd);
  open_library (filename);

  return 0;
}
