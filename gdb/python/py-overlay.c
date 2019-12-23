/* Python interface to overlay manager

   Copyright (C) 2019 Free Software Foundation, Inc.

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

#include "defs.h"
#include "python-internal.h"
#include "python.h"
#include "overlay.h"
#include "arch-utils.h"

/* Constants for method names defined on a Python class.  */
#define EVENT_SYMBOL_NAME_METHOD "event_symbol_name"
#define READ_MAPPINGS_METHOD "read_mappings"
#define ADD_MAPPING_METHOD "add_mapping"

/* Declare. */
struct gdbpy_ovly_mgr_object;
static PyObject * py_overlay_manager_add_mapping (PyObject *self,
						  PyObject *args,
						  PyObject *kwargs);

/* An implementation of an overlay manager that delegates out to Python
   code that the user can easily override.  */

class gdb_py_overlay_manager : public gdb_overlay_manager
{
public:
  gdb_py_overlay_manager (gdbpy_ovly_mgr_object *obj, bool reload_on_event)
    : gdb_overlay_manager (reload_on_event),
      m_obj (obj)
  {
    Py_INCREF (m_obj);
  }

  ~gdb_py_overlay_manager ()
  {
    gdb_assert (gdb_python_initialized);
    gdbpy_enter enter_py (python_gdbarch, current_language);

    Py_DECREF (m_obj);
  }

  std::string event_symbol_name () const override
  {
    gdb_assert (gdb_python_initialized);
    gdbpy_enter enter_py (get_current_arch (), current_language);

    PyObject *obj = (PyObject *) m_obj;

    /* The base class gdb.OverlayManager provides a default implementation
       so this method should always be found.  */
    static const char *method_name = EVENT_SYMBOL_NAME_METHOD;
    gdb_assert (PyObject_HasAttrString (obj, method_name));
    gdbpy_ref<> result (PyObject_CallMethod (obj, method_name, NULL));
    if (result == NULL)
      return "";

    gdb::unique_xmalloc_ptr<char>
      symbol_name (python_string_to_host_string (result.get ()));
    if (symbol_name == NULL)
      return "";

    std::string tmp (symbol_name.get ());
    return tmp;
  }

  std::unique_ptr<std::vector<mapping>> read_mappings () override
  {
    gdb_assert (gdb_python_initialized);
    gdbpy_enter enter_py (get_current_arch (), current_language);

    m_mappings.reset (new std::vector<mapping>);
    PyObject *obj = (PyObject *) m_obj;

    /* The base class gdb.OverlayManager provides a default implementation
       so this method should always be found.  */
    static const char *method_name = READ_MAPPINGS_METHOD;
    gdb_assert (PyObject_HasAttrString (obj, method_name));
    gdbpy_ref<> result (PyObject_CallMethod (obj, method_name, NULL));
    if (result == NULL || !PyObject_IsTrue (result.get ()))
      {
	/* TODO: We get here if the call to read_mappings failed.  We're
	   about to return an empty list of mappings, having ignored any
	   errors, but maybe we should do more to pass error back up the
	   stack?  */

	/* Failed to read current mappings, discard any partial mappings we
	   found and return the empty vector.  */
	m_mappings->clear ();
	return std::move (m_mappings);
      }

    /* Return the vector of mappings we created.  */
    return std::move (m_mappings);
  }

private:

  void add_mapping (CORE_ADDR src, CORE_ADDR dst, ULONGEST len)
  {
    /* TODO: Maybe we should throw an error in this case rather than just
       ignoring the attempt to add a new mapping.  */
    if (m_mappings == nullptr)
      return;

    fprintf (stderr, "py_overlay_manager_add_mapping, src = %s, dst = %s, len = %s\n",
	     core_addr_to_string (src), core_addr_to_string (dst),
	     pulongest (len));
  }

  friend PyObject * py_overlay_manager_add_mapping (PyObject *self, PyObject *args, PyObject *kwargs);

  /* The Python object associated with this overlay manager.  */
  gdbpy_ovly_mgr_object *m_obj;

  /* This vector is non-null only for the duration of read_mappings, and
     is added to by calls to add_mapping.  */
  std::unique_ptr<std::vector<mapping>> m_mappings;
};

/* Wrapper around a Python object, provides a mechanism to find the overlay
   manager object from the Python object.  */

struct gdbpy_ovly_mgr_object {
  /* Python boilerplate, must come first.  */
  PyObject_HEAD

  /* Point at the actual overlay manager we created when this Python object
     was created.  This object is owned by the generic overlay management
     code within GDB.  */
  gdb_py_overlay_manager *manager;
};

/* Initializer for OverlayManager object, it takes no parameters.  */

static int
py_overlay_manager_init (PyObject *self, PyObject *args, PyObject *kwargs)
{
  static const char *keywords[] = { "reload_on_event", NULL };
  PyObject *reload_on_event_obj = NULL;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kwargs, "O", keywords,
					&reload_on_event_obj))
    return -1;

  int reload_on_event = PyObject_IsTrue (reload_on_event_obj);
  if (reload_on_event == -1)
    return -1;

  gdbpy_ovly_mgr_object *obj = (gdbpy_ovly_mgr_object *) self;
  std::unique_ptr <gdb_py_overlay_manager> mgr
    (new gdb_py_overlay_manager (obj, reload_on_event));
  obj->manager = mgr.get ();
  overlay_manager_register (std::move (mgr));
  return 0;
}

/* Deallocate OverlayManager object.  */

static void
py_overlay_manager_dealloc (PyObject *self)
{
  /* TODO: Should ensure that this object is no longer registered as the
     overlay manager for GDB otherwise bad things will happen.  */
  fprintf (stderr, "Deallocate gdb.OverlayManager object\n");

  /* Set this pointer to null not because we have to, but to protect
     against any uses after we deallocate.  */
  gdbpy_ovly_mgr_object *obj = (gdbpy_ovly_mgr_object *) self;
  obj->manager = nullptr;

  /* Now ask Python to free this object.  */
  Py_TYPE (self)->tp_free (self);
}

/* Python function which returns the name of the overlay event symbol.
   This is the fallback, users should be overriding this method.  If we
   get here then return None to indicate that there is no event symbol.  */

static PyObject *
py_overlay_manager_event_symbol_name (PyObject *self, PyObject *args)
{
  Py_RETURN_NONE;
}

/* Default implementation of the "read_mappings" method on the
   gdb.OverlayManager class, this is called if the user provided overlay
   manager doesn't override.  This registers no mappings, and just returns
   None.  */

static PyObject *
py_overlay_manager_read_mappings (PyObject *self, PyObject *args)
{
  Py_RETURN_NONE;
}

/* TODO */

static PyObject *
py_overlay_manager_add_mapping (PyObject *self, PyObject *args, PyObject *kwargs)
{
  gdbpy_ovly_mgr_object *obj = (gdbpy_ovly_mgr_object *) self;

  static const char *keywords[] = { "src", "dst", "len", NULL };
  PyObject *src_obj, *dst_obj, *len_obj;
  CORE_ADDR src, dst;
  ULONGEST len;

  if (!gdb_PyArg_ParseTupleAndKeywords (args, kwargs, "OOO", keywords,
					&src_obj, &dst_obj, &len_obj))
    return nullptr;

  if (get_addr_from_python (src_obj, &src) < 0)
    return nullptr;

  if (get_addr_from_python (dst_obj, &dst) < 0)
    return nullptr;

  if (PyLong_Check (len_obj))
    len = PyLong_AsUnsignedLongLong (len_obj);
  else if (PyInt_Check (len_obj))
    len = (ULONGEST) PyInt_AsLong (len_obj);
  else
    {
      PyErr_SetString (PyExc_TypeError, _("Invalid length argument."));
      return nullptr;
    }

  obj->manager->add_mapping (src, dst, len);
  Py_RETURN_NONE;
}

/* Methods on gdb.OverlayManager.  */

static PyMethodDef overlay_manager_object_methods[] =
{
  { EVENT_SYMBOL_NAME_METHOD, py_overlay_manager_event_symbol_name,
    METH_NOARGS, "Return a string, the name of the event symbol." },
  { READ_MAPPINGS_METHOD, py_overlay_manager_read_mappings,
    METH_NOARGS, "Register the current overlay mappings." },
  { ADD_MAPPING_METHOD, (PyCFunction) py_overlay_manager_add_mapping,
    METH_VARARGS | METH_KEYWORDS,
    "Callback to register a single overlay mapping." },
  { NULL } /* Sentinel.  */
};

void
py_overlay_manager_finalize (void)
{
  overlay_manager_register (nullptr);
}

/* Structure defining an OverlayManager object type.  */

PyTypeObject overlay_manager_object_type =
{
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.OverlayManager",		  /*tp_name*/
  sizeof (gdbpy_ovly_mgr_object), /*tp_basicsize*/
  0,				  /*tp_itemsize*/
  py_overlay_manager_dealloc,	  /*tp_dealloc*/
  0,				  /*tp_print*/
  0,				  /*tp_getattr*/
  0,				  /*tp_setattr*/
  0,				  /*tp_compare*/
  0,				  /*tp_repr*/
  0,				  /*tp_as_number*/
  0,				  /*tp_as_sequence*/
  0,				  /*tp_as_mapping*/
  0,				  /*tp_hash */
  0,				  /*tp_call*/
  0,				  /*tp_str*/
  0,				  /*tp_getattro*/
  0,				  /*tp_setattro */
  0,				  /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,  /*tp_flags*/
  "GDB overlay manager object",	  /* tp_doc */
  0,				  /* tp_traverse */
  0,				  /* tp_clear */
  0,				  /* tp_richcompare */
  0,				  /* tp_weaklistoffset */
  0,				  /* tp_iter */
  0,				  /* tp_iternext */
  overlay_manager_object_methods, /* tp_methods */
  0,				  /* tp_members */
  0,				  /* tp_getset */
  0,				  /* tp_base */
  0,				  /* tp_dict */
  0,				  /* tp_descr_get */
  0,				  /* tp_descr_set */
  0,				  /* tp_dictoffset */
  py_overlay_manager_init,	  /* tp_init */
  0,				  /* tp_alloc */
};

/* Initialize the Python overlay code.  */
int
gdbpy_initialize_overlay (void)
{
  overlay_manager_object_type.tp_new = PyType_GenericNew;
  if (PyType_Ready (&overlay_manager_object_type) < 0)
    return -1;

  if (gdb_pymodule_addobject (gdb_module, "OverlayManager",
			      (PyObject *) &overlay_manager_object_type) < 0)
    return -1;
  return 0;
}
