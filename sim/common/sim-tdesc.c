/* Simulator target description support.
   Copyright (C) 2026 Free Software Foundation, Inc.

This file is part of GDB, the GNU debugger.

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

/* This must come before any other includes.  */
#include "defs.h"

#include <stdio.h>
#include <string.h>

#include "libiberty.h"
#include "sim-main.h"
#include "sim-tdesc.h"

/* Helper to append a string to a dynamically growing buffer.  */

static void
append_string (char **bufp, size_t *sizep, size_t *lenp, const char *str)
{
  size_t str_len = strlen (str);

  /* Grow buffer if needed.  */
  while (*lenp + str_len + 1 > *sizep)
    {
      *sizep = *sizep ? *sizep * 2 : 1024;
      *bufp = xrealloc (*bufp, *sizep);
    }

  memcpy (*bufp + *lenp, str, str_len + 1);
  *lenp += str_len;
}

/* Helper to append a formatted string.  */

static void
append_format (char **bufp, size_t *sizep, size_t *lenp,
	       const char *fmt, ...)
{
  char tmp[256];
  va_list ap;

  va_start (ap, fmt);
  vsnprintf (tmp, sizeof (tmp), fmt, ap);
  va_end (ap);

  append_string (bufp, sizep, lenp, tmp);
}

/* Convert a target description structure to an XML string.
   The caller is responsible for freeing the returned string.
   Returns NULL if TDESC is NULL.  */

static char *
sim_tdesc_to_xml (const struct sim_tdesc *tdesc)
{
  char *buf = NULL;
  size_t size = 0;
  size_t len = 0;
  int regnum = 0;
  int i, j;

  if (tdesc == NULL)
    return NULL;

  /* XML header.  */
  append_string (&buf, &size, &len,
    "<?xml version=\"1.0\"?>\n"
    "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">\n"
    "<target version=\"1.0\">\n");

  /* Architecture.  */
  if (tdesc->arch != NULL)
    append_format (&buf, &size, &len, "  <architecture>%s</architecture>\n",
		   tdesc->arch);

  /* Features.  */
  for (i = 0; i < tdesc->num_features; i++)
    {
      const struct sim_tdesc_feature *feat = &tdesc->features[i];
      int k;

      append_format (&buf, &size, &len, "  <feature name=\"%s\">\n",
		     feat->name);

      /* Emit types first (they must be defined before use).  */
      for (j = 0; j < feat->num_types; j++)
	{
	  const struct sim_tdesc_type *type = &feat->types[j];
	  const char *tag_name;

	  switch (type->kind)
	    {
	    case SIM_TDESC_TYPE_VECTOR:
	      /* Vector: <vector id="..." type="..." count="..."/> */
	      append_format (&buf, &size, &len,
			     "    <vector id=\"%s\" type=\"%s\" count=\"%d\"/>\n",
			     type->id, type->element_type, type->count);
	      break;

	    case SIM_TDESC_TYPE_UNION:
	    case SIM_TDESC_TYPE_STRUCT:
	    case SIM_TDESC_TYPE_FLAGS:
	      /* Determine XML tag name.  */
	      if (type->kind == SIM_TDESC_TYPE_UNION)
		tag_name = "union";
	      else if (type->kind == SIM_TDESC_TYPE_STRUCT)
		tag_name = "struct";
	      else
		tag_name = "flags";

	      /* Opening tag with optional size for bitfield types.  */
	      if (type->size > 0)
		append_format (&buf, &size, &len,
			       "    <%s id=\"%s\" size=\"%d\">\n",
			       tag_name, type->id, type->size);
	      else
		append_format (&buf, &size, &len, "    <%s id=\"%s\">\n",
			       tag_name, type->id);

	      /* Emit fields.  */
	      for (k = 0; k < type->num_fields; k++)
		{
		  const struct sim_tdesc_type_field *field = &type->fields[k];

		  if (field->start >= 0)
		    {
		      /* Bitfield: name, start, end, optional type.  */
		      if (field->type != NULL)
			append_format (&buf, &size, &len,
				       "      <field name=\"%s\" start=\"%d\" "
				       "end=\"%d\" type=\"%s\"/>\n",
				       field->name, field->start, field->end,
				       field->type);
		      else
			append_format (&buf, &size, &len,
				       "      <field name=\"%s\" start=\"%d\" "
				       "end=\"%d\"/>\n",
				       field->name, field->start, field->end);
		    }
		  else
		    {
		      /* Non-bitfield: name and type.  */
		      append_format (&buf, &size, &len,
				     "      <field name=\"%s\" type=\"%s\"/>\n",
				     field->name, field->type);
		    }
		}

	      /* Closing tag.  */
	      append_format (&buf, &size, &len, "    </%s>\n", tag_name);
	      break;
	    }
	}

      /* Emit registers.  */
      for (j = 0; j < feat->num_regs; j++)
	{
	  const struct sim_tdesc_reg *reg = &feat->regs[j];
	  int rn = (reg->regnum >= 0) ? reg->regnum : regnum;
	  const char *type = reg->type ? reg->type : "int";

	  append_format (&buf, &size, &len,
			 "    <reg name=\"%s\" bitsize=\"%d\" regnum=\"%d\" "
			 "type=\"%s\"/>\n",
			 reg->name, reg->bitsize, rn, type);

	  regnum = rn + 1;
	}

      append_string (&buf, &size, &len, "  </feature>\n");
    }

  append_string (&buf, &size, &len, "</target>\n");

  return buf;
}

/* See include/sim/sim.h.  */

char *
sim_target_description (SIM_DESC sd)
{
  const struct sim_tdesc *tdesc = NULL;

  if (STATE_TDESC_GET (sd) != NULL)
    tdesc = STATE_TDESC_GET (sd) (sd);

  return sim_tdesc_to_xml (tdesc);
}
