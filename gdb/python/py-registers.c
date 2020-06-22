/* Python interface to register, and register group information.

   Copyright (C) 2020 Free Software Foundation, Inc.

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
#include "gdbarch.h"
#include "arch-utils.h"
#include "disasm.h"
#include "reggroups.h"
#include "python-internal.h"

/* Structure for iterator over register descriptors.  */
typedef struct {
  PyObject_HEAD

  /* The register group that the user is iterating over.  This will never
     be NULL.  */
  struct reggroup *reggroup;

  /* The next register number to lookup.  Starts at 0 and counts up.  */
  int regnum;

  /* Pointer back to the architecture we're finding registers for.  */
  struct gdbarch *gdbarch;
} register_descriptor_iterator_object;

extern PyTypeObject register_descriptor_iterator_object_type
    CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF ("register_descriptor_iterator_object");

/* A register descriptor.  */
typedef struct {
  PyObject_HEAD

  /* The register this is a descriptor for.  */
  int regnum;

  /* The architecture this is a register for.  */
  struct gdbarch *gdbarch;
} register_descriptor_object;

extern PyTypeObject register_descriptor_object_type
    CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF ("register_descriptor_object");

/* Structure for iterator over register groups.  */
typedef struct {
  PyObject_HEAD

  /* The last register group returned.  Initially this will be NULL.  */
  struct reggroup *reggroup;

  /* Pointer back to the architecture we're finding registers for.  */
  struct gdbarch *gdbarch;
} reggroup_iterator_object;

extern PyTypeObject reggroup_iterator_object_type
    CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF ("reggroup_iterator_object");

/* A register group object.  */
typedef struct {
  PyObject_HEAD

  /* The register group being described.  */
  struct reggroup *reggroup;
} reggroup_object;

extern PyTypeObject reggroup_object_type
    CPYCHECKER_TYPE_OBJECT_FOR_TYPEDEF ("reggroup_object");

/* Create a new gdb.RegisterGroup object wrapping REGGROUP.  */

static PyObject *
gdbpy_new_reggroup (struct reggroup *reggroup)
{
  /* Create a new object and fill in its details.  */
  reggroup_object *group
    = PyObject_New (reggroup_object, &reggroup_object_type);
  if (group == NULL)
    return NULL;
  group->reggroup = reggroup;
  return (PyObject *) group;
}

/* Convert a gdb.RegisterGroup to a string, it just returns the name of
   the register group.  */

static PyObject *
gdbpy_reggroup_to_string (PyObject *self)
{
  reggroup_object *group = (reggroup_object *) self;
  struct reggroup *reggroup = group->reggroup;

  const char *name = reggroup_name (reggroup);
  return PyString_FromString (name);
}

/* Implement gdb.RegisterGroup.name (self) -> String.
   Return a string that is the name of this register group.  */

static PyObject *
gdbpy_reggroup_name (PyObject *self, void *closure)
{
  return gdbpy_reggroup_to_string (self);
}

/* Create an return a new gdb.RegisterDescriptor object.  */
static PyObject *
gdbpy_new_register_descriptor (struct gdbarch *gdbarch,
			       int regnum)
{
  /* Create a new object and fill in its details.  */
  register_descriptor_object *reg
    = PyObject_New (register_descriptor_object,
		    &register_descriptor_object_type);
  if (reg == NULL)
    return NULL;
  reg->regnum = regnum;
  reg->gdbarch = gdbarch;
  return (PyObject *) reg;
}

/* Convert the register descriptor to a string.  */

static PyObject *
gdbpy_register_descriptor_to_string (PyObject *self)
{
  register_descriptor_object *reg
    = (register_descriptor_object *) self;
  struct gdbarch *gdbarch = reg->gdbarch;
  int regnum = reg->regnum;

  const char *name = gdbarch_register_name (gdbarch, regnum);
  return PyString_FromString (name);
}

/* Implement gdb.RegisterDescriptor.name attribute get function.  Return a
   string that is the name of this register.  Due to checking when register
   descriptors are created the name will never by the empty string.  */

static PyObject *
gdbpy_register_descriptor_name (PyObject *self, void *closure)
{
  return gdbpy_register_descriptor_to_string (self);
}

/* Return a reference to the gdb.RegisterGroupsIterator object.  */

static PyObject *
gdbpy_reggroup_iter (PyObject *self)
{
  Py_INCREF (self);
  return self;
}

/* Return the next gdb.RegisterGroup object from the iterator.  */

static PyObject *
gdbpy_reggroup_iter_next (PyObject *self)
{
  reggroup_iterator_object *iter_obj
    = (reggroup_iterator_object *) self;
  struct gdbarch *gdbarch = iter_obj->gdbarch;

  struct reggroup *next_group = reggroup_next (gdbarch, iter_obj->reggroup);
  if (next_group == NULL)
    {
      PyErr_SetString (PyExc_StopIteration, _("No more groups"));
      return NULL;
    }

  iter_obj->reggroup = next_group;
  return gdbpy_new_reggroup (iter_obj->reggroup);
}

/* Return a new gdb.RegisterGroupsIterator over all the register groups in
   GDBARCH.  */

PyObject *
gdbpy_new_reggroup_iterator (struct gdbarch *gdbarch)
{
  gdb_assert (gdbarch != nullptr);

  /* Create a new object and fill in its internal state.  */
  reggroup_iterator_object *iter
    = PyObject_New (reggroup_iterator_object,
		    &reggroup_iterator_object_type);
  if (iter == NULL)
    return NULL;
  iter->reggroup = NULL;
  iter->gdbarch = gdbarch;
  return (PyObject *) iter;
}

/* Create and return a new gdb.RegisterDescriptorIterator object which
   will iterate over all registers in GROUP_NAME for GDBARCH.  If
   GROUP_NAME is either NULL or the empty string then the ALL_REGGROUP is
   used, otherwise lookup the register group matching GROUP_NAME and use
   that.

   This function can return NULL if GROUP_NAME isn't found.  */

PyObject *
gdbpy_new_register_descriptor_iterator (struct gdbarch *gdbarch,
					const char *group_name)
{
  struct reggroup *grp = NULL;

  /* Lookup the requested register group, or find the default.  */
  if (group_name == NULL || *group_name == '\0')
    grp = all_reggroup;
  else
    {
      grp = reggroup_find (gdbarch, group_name);
      if (grp == NULL)
	{
	  PyErr_SetString (PyExc_ValueError,
			   _("Unknown register group name."));
	  return NULL;
	}
    }
  /* Create a new iterator object initialised for this architecture and
     fill in all of the details.  */
  register_descriptor_iterator_object *iter
    = PyObject_New (register_descriptor_iterator_object,
		    &register_descriptor_iterator_object_type);
  if (iter == NULL)
    return NULL;
  iter->regnum = 0;
  iter->gdbarch = gdbarch;
  gdb_assert (grp != NULL);
  iter->reggroup = grp;

  return (PyObject *) iter;
}

/* Return a reference to the gdb.RegisterDescriptorIterator object.  */

static PyObject *
gdbpy_register_descriptor_iter (PyObject *self)
{
  Py_INCREF (self);
  return self;
}

/* Return the next register name.  */

static PyObject *
gdbpy_register_descriptor_iter_next (PyObject *self)
{
  register_descriptor_iterator_object *iter_obj
    = (register_descriptor_iterator_object *) self;
  struct gdbarch *gdbarch = iter_obj->gdbarch;

  do
    {
      if (iter_obj->regnum >= gdbarch_num_cooked_regs (gdbarch))
	{
	  PyErr_SetString (PyExc_StopIteration, _("No more registers"));
	  return NULL;
	}

      const char *name = nullptr;
      int regnum = iter_obj->regnum;
      if (gdbarch_register_reggroup_p (gdbarch, regnum,
				       iter_obj->reggroup))
	name = gdbarch_register_name (gdbarch, regnum);
      iter_obj->regnum++;

      if (name != nullptr && *name != '\0')
	return gdbpy_new_register_descriptor (gdbarch, regnum);
    }
  while (true);
}

/* Initializes the new Python classes from this file in the gdb module.  */

int
gdbpy_initialize_registers ()
{
  register_descriptor_object_type.tp_new = PyType_GenericNew;
  if (PyType_Ready (&register_descriptor_object_type) < 0)
    return -1;
  if (gdb_pymodule_addobject
      (gdb_module, "RegisterDescriptor",
       (PyObject *) &register_descriptor_object_type) < 0)
    return -1;

  reggroup_iterator_object_type.tp_new = PyType_GenericNew;
  if (PyType_Ready (&reggroup_iterator_object_type) < 0)
    return -1;
  if (gdb_pymodule_addobject
      (gdb_module, "RegisterGroupsIterator",
       (PyObject *) &reggroup_iterator_object_type) < 0)
    return -1;

  reggroup_object_type.tp_new = PyType_GenericNew;
  if (PyType_Ready (&reggroup_object_type) < 0)
    return -1;
  if (gdb_pymodule_addobject
      (gdb_module, "RegisterGroup",
       (PyObject *) &reggroup_object_type) < 0)
    return -1;

  register_descriptor_iterator_object_type.tp_new = PyType_GenericNew;
  if (PyType_Ready (&register_descriptor_iterator_object_type) < 0)
    return -1;
  return (gdb_pymodule_addobject
	  (gdb_module, "RegisterDescriptorIterator",
	   (PyObject *) &register_descriptor_iterator_object_type));
}

PyTypeObject register_descriptor_iterator_object_type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.RegisterDescriptorIterator",	  	/*tp_name*/
  sizeof (register_descriptor_iterator_object),	/*tp_basicsize*/
  0,				  /*tp_itemsize*/
  0,				  /*tp_dealloc*/
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
  0,				  /*tp_setattro*/
  0,				  /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,			/*tp_flags*/
  "GDB architecture register descriptor iterator object",	/*tp_doc */
  0,				  /*tp_traverse */
  0,				  /*tp_clear */
  0,				  /*tp_richcompare */
  0,				  /*tp_weaklistoffset */
  gdbpy_register_descriptor_iter,	  /*tp_iter */
  gdbpy_register_descriptor_iter_next,  /*tp_iternext */
  0				  /*tp_methods */
};

static gdb_PyGetSetDef gdbpy_register_descriptor_getset[] = {
  { "name", gdbpy_register_descriptor_name, NULL,
    "The name of this register.", NULL },
  { NULL }  /* Sentinel */
};

PyTypeObject register_descriptor_object_type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.RegisterDescriptor",	  /*tp_name*/
  sizeof (register_descriptor_object),	/*tp_basicsize*/
  0,				  /*tp_itemsize*/
  0,				  /*tp_dealloc*/
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
  gdbpy_register_descriptor_to_string,			/*tp_str*/
  0,				  /*tp_getattro*/
  0,				  /*tp_setattro*/
  0,				  /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,		  /*tp_flags*/
  "GDB architecture register descriptor object",	/*tp_doc */
  0,				  /*tp_traverse */
  0,				  /*tp_clear */
  0,				  /*tp_richcompare */
  0,				  /*tp_weaklistoffset */
  0,				  /*tp_iter */
  0,				  /*tp_iternext */
  0,				  /*tp_methods */
  0,				  /*tp_members */
  gdbpy_register_descriptor_getset			/*tp_getset */
};

PyTypeObject reggroup_iterator_object_type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.RegisterGroupsIterator",	  /*tp_name*/
  sizeof (reggroup_iterator_object),		/*tp_basicsize*/
  0,				  /*tp_itemsize*/
  0,				  /*tp_dealloc*/
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
  0,				  /*tp_setattro*/
  0,				  /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,	/*tp_flags*/
  "GDB register groups iterator object",	/*tp_doc */
  0,				  /*tp_traverse */
  0,				  /*tp_clear */
  0,				  /*tp_richcompare */
  0,				  /*tp_weaklistoffset */
  gdbpy_reggroup_iter,		  /*tp_iter */
  gdbpy_reggroup_iter_next,	  /*tp_iternext */
  0				  /*tp_methods */
};

static gdb_PyGetSetDef gdbpy_reggroup_getset[] = {
  { "name", gdbpy_reggroup_name, NULL,
    "The name of this register group.", NULL },
  { NULL }  /* Sentinel */
};

PyTypeObject reggroup_object_type = {
  PyVarObject_HEAD_INIT (NULL, 0)
  "gdb.RegisterGroup",		  /*tp_name*/
  sizeof (reggroup_object),	  /*tp_basicsize*/
  0,				  /*tp_itemsize*/
  0,				  /*tp_dealloc*/
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
  gdbpy_reggroup_to_string,	  /*tp_str*/
  0,				  /*tp_getattro*/
  0,				  /*tp_setattro*/
  0,				  /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,		  /*tp_flags*/
  "GDB register group object",	  /*tp_doc */
  0,				  /*tp_traverse */
  0,				  /*tp_clear */
  0,				  /*tp_richcompare */
  0,				  /*tp_weaklistoffset */
  0,				  /*tp_iter */
  0,				  /*tp_iternext */
  0,				  /*tp_methods */
  0,				  /*tp_members */
  gdbpy_reggroup_getset		  /*tp_getset */
};
