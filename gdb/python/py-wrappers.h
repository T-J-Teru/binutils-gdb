/* Wrappers for some Python safety.

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

#ifndef GDB_PYTHON_PY_WRAPPERS_H
#define GDB_PYTHON_PY_WRAPPERS_H

#include "py-ref.h"

/* Gdb implements its own wrappers for many Python APIs.  This is done
   in an attempt to be more safe.

   In particular, in gdb:

   - APIs returning a new reference will return gdbpy_ref<>.  This
   makes reference counting errors less likely.

   - APIs will throw an exception rather than return a special value
   (NULL or -1).  This makes error checking simpler.

   - APIs requiring a stolen reference take a gdbpy_ref<>&&, to make
   reference counting errors less likely.

   This file holds the currently-defined wrappers.  If new APIs are
   needed, the normal approach is to add a wrapper here.

   APIs here are named after the underlying Python function, but using
   lower case and an "_" at each word break.  */

/* The type of exception thrown when the Python exception has been
   set.  */
struct gdb_python_exception
{
  gdb_python_exception ()
  {
    gdb_assert (PyErr_Occurred ());
  }
};

template<typename T>
gdbpy_ref<T>
gdbpy_new ()
{
  gdbpy_ref<T> result (PyObject_New (T, T::corresponding_object_type));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_bool_from_long (long value)
{
  /* This cannot fail.  */
  return gdbpy_ref<> (PyBool_FromLong (value));
}

static inline char *
gdbpy_bytes_as_string (gdbpy_borrowed_ref ref)
{
  char *result = PyBytes_AsString (ref);
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline void
gdbpy_bytes_as_string_and_size (gdbpy_borrowed_ref ref,
				char **buffer,
				Py_ssize_t *length)
{
  if (PyBytes_AsStringAndSize (ref, buffer, length) == -1)
    throw gdb_python_exception ();
}

static inline gdbpy_ref<>
gdbpy_bytes_from_string (const char *str)
{
  gdb_assert (str != nullptr);
  gdbpy_ref<> result (PyBytes_FromString (str));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_bytes_from_string_and_size (const char *str, Py_ssize_t len)
{
  /* Python allows STR==nullptr but it leaves the object
     uninitialized, and I think we should avoid this in gdb.  */
  gdb_assert (str != nullptr);
  gdbpy_ref<> result (PyBytes_FromStringAndSize (str, len));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline Py_ssize_t
gdbpy_bytes_size (gdbpy_borrowed_ref ref)
{
  Py_ssize_t result = PyBytes_Size (ref);
  if (result == -1)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_new_list (Py_ssize_t len)
{
  gdbpy_ref<> result (PyList_New (len));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline void
gdbpy_list_append (gdbpy_borrowed_ref list, gdbpy_borrowed_ref val)
{
  if (PyList_Append (list, val) < 0)
    throw gdb_python_exception ();
}

static inline gdbpy_ref<>
gdbpy_new_dict ()
{
  gdbpy_ref<> result (PyDict_New ());
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline void
gdbpy_dict_set_item_string (gdbpy_borrowed_ref dict,
			    const char *key,
			    gdbpy_borrowed_ref value)
{
  if (PyDict_SetItemString (dict, key, value) != 0)
    throw gdb_python_exception ();
}

static inline void
gdbpy_dict_del_item_string (gdbpy_borrowed_ref dict,
			    const char *key)
{
  if (PyDict_DelItemString (dict, key) == -1)
    throw gdb_python_exception ();
}

static inline gdbpy_borrowed_ref
gdbpy_dict_get_item_with_error (gdbpy_borrowed_ref dict,
				gdbpy_borrowed_ref key)
{
  PyObject *result = PyDict_GetItemWithError (dict, key);
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_dict_keys (gdbpy_borrowed_ref dict)
{
  gdbpy_ref<> result (PyDict_Keys (dict));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_unicode_from_string (std::string_view str)
{
  gdbpy_ref<> result (PyUnicode_FromStringAndSize (str.data (), str.size ()));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_unicode_from_format (const char *fmt, ...)
{
  va_list args;
  va_start (args, fmt);
  gdbpy_ref<> result (PyUnicode_FromFormatV (fmt, args));
  if (result == nullptr)
    throw gdb_python_exception ();
  va_end (args);
  return result;
}

[[noreturn]] static inline void
gdbpy_err_set_string (gdbpy_borrowed_ref type, const char *str)
{
  PyErr_SetString (type, str);
  throw gdb_python_exception ();
}

/* This is a template because PyErr_FormatV was only added in Python
   3.5.  */
template<typename... Arg>
[[noreturn]] void
gdbpy_err_format (gdbpy_borrowed_ref type, const char *fmt, Arg... args)
{
  PyErr_Format (type, fmt, std::forward<Arg> (args)...);
  throw gdb_python_exception ();
}

template<typename... Arg>
void
gdbpy_arg_parse_tuple_and_keywords (gdbpy_borrowed_ref args,
				    gdbpy_opt_borrowed_ref kw,
				    const char *fmt,
				    const char **keywords,
				    Arg... outputs)
{
  /* It would be cool if callers could use references to the
     out-parameters and also if gdbpy_borrowed_ref could be used for
     those.  That requires some hairy template metaprogramming
     though.  */
  if (!gdb_PyArg_ParseTupleAndKeywords (args, kw, fmt, keywords,
					std::forward<Arg> (outputs)...))
    throw gdb_python_exception ();
}

static inline void
gdbpy_arg_parse_tuple (gdbpy_borrowed_ref param, const char *format, ...)
{
  va_list args;
  va_start (args, format);
  if (!PyArg_VaParse (param, format, args))
    throw gdb_python_exception ();
  va_end (args);
}

static inline long
gdbpy_long_as_long (gdbpy_borrowed_ref arg)
{
  long result = PyLong_AsLong (arg);
  if (result == -1 && PyErr_Occurred ())
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_tuple_new (Py_ssize_t len)
{
  gdbpy_ref<> result (PyTuple_New (len));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_borrowed_ref
gdbpy_tuple_get_item (gdbpy_borrowed_ref tuple, Py_ssize_t pos)
{
  PyObject *result = PyTuple_GetItem (tuple, pos);
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline void
gdbpy_tuple_set_item (gdbpy_borrowed_ref tuple, Py_ssize_t pos,
		      gdbpy_ref<> &&item)
{
  if (PyTuple_SetItem (tuple, pos, item.release ()) == -1)
    throw gdb_python_exception ();
}

static inline Py_ssize_t
gdbpy_tuple_size (gdbpy_borrowed_ref tuple)
{
  Py_ssize_t result = PyTuple_Size (tuple);
  if (result == -1)
    throw gdb_python_exception ();
  return result;
}

static inline Py_ssize_t
gdbpy_sequence_size (gdbpy_borrowed_ref seq)
{
  Py_ssize_t result = PySequence_Size (seq);
  if (result == -1)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_sequence_get_item (gdbpy_borrowed_ref seq, Py_ssize_t i)
{
  gdbpy_ref<> result (PySequence_GetItem (seq, i));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline void
gdbpy_sequence_del_item (gdbpy_borrowed_ref seq, Py_ssize_t i)
{
  if (PySequence_DelItem (seq, i) == -1)
    throw gdb_python_exception ();
}

static inline gdbpy_ref<>
gdbpy_sequence_list (gdbpy_borrowed_ref seq)
{
  gdbpy_ref<> result (PySequence_List (seq));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

static inline gdbpy_ref<>
gdbpy_sequence_concat (gdbpy_borrowed_ref first, gdbpy_borrowed_ref second)
{
  gdbpy_ref<> result (PySequence_Concat (first, second));
  if (result == nullptr)
    throw gdb_python_exception ();
  return result;
}

#endif /* GDB_PYTHON_PY_WRAPPERS_H */
