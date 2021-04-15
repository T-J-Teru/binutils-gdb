/* Filename-seen cache for the GNU debugger, GDB.

   Copyright (C) 1986-2021 Free Software Foundation, Inc.

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

#ifndef FILENAME_SEEN_CACHE_H
#define FILENAME_SEEN_CACHE_H

#include "defs.h"
#include "gdbsupport/function-view.h"
#include "filenames.h"

#include <unordered_map>
#include <unordered_set>

namespace filename_seen_cache_impl
{

/* Used for hashing filenames.  */

struct filename_hasher
{
  std::size_t
  operator() (const char * const &filename) const
  {
    return (std::size_t) filename_hash (filename);
  }
};

/* Use to compare two filenames for equality.  */

struct filename_equality
{
  bool
  operator() (const char * const &f1, const char * const&f2) const
  {
    return (bool) filename_eq (f1, f2);
  }
};

/* A version of filename_seen_cache that stores the filenames in an
   unordered_set.  No additional information is stored alongside the
   filename.  */
struct filename_seen_set_cache
{
  filename_seen_set_cache () = default;

  DISABLE_COPY_AND_ASSIGN (filename_seen_set_cache);

  /* If FILE is not already in the table of files in CACHE, add it and
     return false; otherwise return true.

     NOTE: We don't manage space for FILE, we assume FILE lives as
     long as the caller needs.  */
  bool seen (const char *filename)
  {
    bool added = m_set.insert (filename).second;
    return !added;
  }

  /* Empty the cache, but do not delete it.  */
  void clear ()
  {
    m_set.clear ();
  }

  /* Traverse all cache entries, calling CALLBACK on each.  The
     filename is passed as argument to CALLBACK.  */
  void traverse (gdb::function_view<void (const char *filename)> callback)
  {
    for (const auto &filename : m_set)
      callback (filename);
  }

private:
  /* The filenames are stored here.  */
  std::unordered_set<const char *, filename_hasher, filename_equality> m_set;
};

/* A version of filename_seen_cache that stores additional data alongside
   the cached filenames.  */
template<typename... Ts>
struct filename_seen_map_cache
{
  filename_seen_map_cache () = default;

  DISABLE_COPY_AND_ASSIGN (filename_seen_map_cache);

  /* If FILE is not already in the table of files in CACHE, add it and
     return false; otherwise return true.  If FILE is inserted into the
     table then ARGS will be stored too, otherwise ARGS will not be
     stored.

     NOTE: We don't manage space for FILE or ARGS, we assume FILE and ARGS
     lives as long as the caller needs.  */
  bool seen (const char *filename, Ts... args)
  {
    std::tuple<Ts...> data (std::forward<Ts> (args) ...);
    bool added = m_map.insert ({std::move (filename), std::move (data)}).second;
    return !added;
  }

  /* Empty the cache, but do not delete it.  */
  void clear ()
  {
    m_map.clear ();
  }

  /* Traverse all cache entries, calling CALLBACK on each.  The
     filename is passed as argument to CALLBACK as well as all of the
     arguments that were passed to the seen call above.  */
  void traverse (gdb::function_view<void (const char *filename, Ts... args)> callback)
  {
    for (const auto &entry : m_map)
      {
        const char *filename = entry.first;
        std::tuple<Ts...> args = entry.second;
        do_callback (callback, filename, args,
                     typename gen_sequence<sizeof... (Ts)>::type ());
      }
  }

private:

  /* Some helper classes used to build a type that allows us to pull apart
     the std::tuple in do_callback.  */
  template<int ...>
  struct sequence
  { /* Nothing.  */ };

  template<int N, int ...S>
  struct gen_sequence : gen_sequence<N-1, N-1, S...>
  { /* Nothing.  */ };

  template<int ...S>
  struct gen_sequence<0, S...>
  {
    typedef sequence<S...> type;
  };

  /* Call CALLBACK passing in FILENAME and all of the components of ARGS as
     separate parameters.  */
  template<int ...S>
  void do_callback
    (gdb::function_view<void (const char *filename, Ts... args)> callback,
     const char *filename, std::tuple<Ts...> args, sequence<S...>)
  {
    callback (filename, std::get<S> (args) ...);
  }

  /* The filenames and arguments are stored here.  */
  std::unordered_map<const char *, std::tuple<Ts...>,
		     filename_hasher, filename_equality> m_map;
};
};

/* Cache to watch for file names already seen.  */
template<typename... Ts>
using filename_seen_cache
	= typename std::conditional<sizeof...(Ts) == 0,
				    filename_seen_cache_impl::filename_seen_set_cache,
				    filename_seen_cache_impl::filename_seen_map_cache<Ts...>>::type;

#endif /* FILENAME_SEEN_CACHE_H */
