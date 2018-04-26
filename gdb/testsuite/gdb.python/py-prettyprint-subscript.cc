/* This testcase is part of GDB, the GNU debugger.

   Copyright 2021 Free Software Foundation, Inc.

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

#include <cstring>
#include <cstdlib>
#include <stdexcept>

template<typename TYPE>
class generic_array
{
public:
  generic_array (const char *name)
    : m_nitems (0),
      m_items (nullptr)
  {
    m_name = strdup (name);
  }

  void
  push_back (TYPE item)
  {
    m_nitems++;
    m_items = (TYPE *) realloc (m_items, (sizeof (TYPE) * m_nitems));
    m_items[m_nitems - 1] = item;
  }

#ifdef DEFINE_SUBSCRIPT_OPERATOR
  TYPE &
  operator[] (int idx)
  {
    if (idx < 0 || idx >= m_nitems)
      throw std::out_of_range ("array index out of bounds");
    return m_items[idx];
  }
#endif

private:
  const char *m_name;
  int m_nitems;
  TYPE *m_items;
};

template<typename K_TYPE, typename V_TYPE>
class generic_map
{
public:
  generic_map (const char *name)
    : m_nitems (0),
      m_items (nullptr)
  {
    m_name = strdup (name);
  }

  void
  insert (K_TYPE key, V_TYPE value)
  {
    for (int i = 0; i < m_nitems; ++i)
      {
	if (m_items[i].key == key)
	  {
	    m_items[i].value = value;
	    return;
	  }
      }

    m_nitems++;
    m_items
      = (map_entry *) realloc (m_items, (sizeof (map_entry)) * m_nitems);
    m_items[m_nitems - 1].key = key;
    m_items[m_nitems - 1].value = value;
}

private:

  struct map_entry
  {
    K_TYPE key;
    V_TYPE value;
  };

  const char *m_name;
  int m_nitems;
  map_entry *m_items;
};

volatile int dump_int;
volatile float dump_float;

int
main ()
{
  generic_array<int> obj_int ("first int array");
  generic_array<float> obj_float ("first float array");
  generic_map<int,int> obj_int_int ("int to int map");

  obj_int.push_back (3);
  obj_int.push_back (6);
  obj_int.push_back (4);
  obj_int.push_back (2);

  obj_float.push_back (3.1);
  obj_float.push_back (6.2);
  obj_float.push_back (4.3);
  obj_float.push_back (2.4);

  obj_int_int.insert (3, 99);
  obj_int_int.insert (5, 21);
  obj_int_int.insert (8, 16);
  obj_int_int.insert (9, 42);

#ifdef DEFINE_SUBSCRIPT_OPERATOR
  dump_int = obj_int[0];
  dump_float = obj_float[0];
#endif

  return 0;	/* Breakpoint 1.  */
}
