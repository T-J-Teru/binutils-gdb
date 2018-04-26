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

#include <string.h>
#include <stdlib.h>

#define CONCAT2(a,b) a##b

#define MAKE_ARRAY_TYPE(TYPENAME,TYPE)				\
  typedef struct TYPENAME					\
  {								\
    const char *m_name;						\
    int m_nitems;						\
    TYPE *m_items;						\
  } TYPENAME;							\
								\
  void								\
  CONCAT2(TYPENAME,_init) (TYPENAME *object, const char *name)	\
  {								\
    object->m_name = strdup (name);				\
    object->m_nitems = 0;					\
    object->m_items = NULL;					\
  }								\
								\
  void								\
  CONCAT2(TYPENAME,_push_back) (TYPENAME *object, TYPE item)	\
  {								\
    object->m_nitems++;						\
    object->m_items						\
      = realloc (object->m_items, (sizeof (TYPE)		\
				   * object->m_nitems));	\
    object->m_items[object->m_nitems - 1] = item;		\
  }

#define MAKE_MAP_TYPE(TYPENAME,K_TYPE,V_TYPE)			\
  struct CONCAT2(TYPENAME,_entry)				\
  {								\
    K_TYPE key;							\
    V_TYPE value;						\
  };								\
								\
  typedef struct TYPENAME					\
  {								\
    const char *m_name;						\
    int m_nitems;						\
    struct CONCAT2(TYPENAME,_entry) *m_items;			\
  } TYPENAME;							\
								\
  void								\
  CONCAT2(TYPENAME,_init) (TYPENAME *object, const char *name)	\
  {								\
    object->m_name = strdup (name);				\
    object->m_nitems = 0;						\
    object->m_items = NULL;					\
  }								\
								\
  void								\
  CONCAT2(TYPENAME,_insert) (TYPENAME *object, K_TYPE key, V_TYPE value) \
  {									\
    int i;								\
									\
    for (i = 0; i < object->m_nitems; ++i)				\
      {									\
	if (object->m_items[i].key == key)				\
	  {								\
	    object->m_items[i].value = value;				\
	    return;							\
	  }								\
      }									\
    									\
    object->m_nitems++;							\
    object->m_items							\
      = realloc (object->m_items, (sizeof (struct CONCAT2(TYPENAME,_entry)) \
				   * object->m_nitems));		\
    object->m_items[object->m_nitems - 1].key = key;			\
    object->m_items[object->m_nitems - 1].value = value;		\
  }

MAKE_ARRAY_TYPE (int_array,int)
MAKE_ARRAY_TYPE (float_array,float)

MAKE_MAP_TYPE (int_int_map,int,int)

int
main ()
{
  int_array obj_int;
  float_array obj_float;
  int_int_map obj_int_int;

  int_array_init (&obj_int, "first int array");
  int_array_push_back (&obj_int, 3);
  int_array_push_back (&obj_int, 6);
  int_array_push_back (&obj_int, 4);
  int_array_push_back (&obj_int, 2);

  float_array_init (&obj_float, "first float array");
  float_array_push_back (&obj_float, 3.1);
  float_array_push_back (&obj_float, 6.2);
  float_array_push_back (&obj_float, 4.3);
  float_array_push_back (&obj_float, 2.4);

  int_int_map_init (&obj_int_int, "int to int map");
  int_int_map_insert (&obj_int_int, 3, 99);
  int_int_map_insert (&obj_int_int, 5, 21);
  int_int_map_insert (&obj_int_int, 8, 16);
  int_int_map_insert (&obj_int_int, 9, 42);

  return 0;	/* Breakpoint 1.  */
}
