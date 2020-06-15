/* Copyright (C) 1986-2020 Free Software Foundation, Inc.

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
#include "gdbcmd.h"
#include "regcache.h"
#include "gdbsupport/def-vector.h"
#include "valprint.h"
#include "remote.h"
#include "reggroups.h"
#include "target.h"
#include "gdbarch.h"

/* Dump registers from regcache, used for dumping raw registers and
   cooked registers.  */

class register_dump_regcache : public register_dump
{
public:
  register_dump_regcache (regcache *regcache, bool dump_pseudo,
			  bool hide_nameless)
    : register_dump (regcache->arch (), hide_nameless),
      m_regcache (regcache),
      m_dump_pseudo (dump_pseudo)
  {
  }

protected:
  void dump_reg (ui_file *file, int regnum) override
  {
    if (regnum < 0)
      {
	if (m_dump_pseudo)
	  fprintf_unfiltered (file, "Cooked value");
	else
	  fprintf_unfiltered (file, "Raw value");
      }
    else
      {
	if (regnum < gdbarch_num_regs (m_gdbarch) || m_dump_pseudo)
	  {
	    auto size = register_size (m_gdbarch, regnum);

	    if (size == 0)
	      return;

	    gdb::def_vector<gdb_byte> buf (size);
	    auto status = m_regcache->cooked_read (regnum, buf.data ());

	    if (status == REG_UNKNOWN)
	      fprintf_unfiltered (file, "<invalid>");
	    else if (status == REG_UNAVAILABLE)
	      fprintf_unfiltered (file, "<unavailable>");
	    else
	      {
		print_hex_chars (file, buf.data (), size,
				 gdbarch_byte_order (m_gdbarch), true);
	      }
	  }
	else
	  {
	    /* Just print "<cooked>" for pseudo register when
	       regcache_dump_raw.  */
	    fprintf_unfiltered (file, "<cooked>");
	  }
      }
  }

private:
  regcache *m_regcache;

  /* Dump pseudo registers or not.  */
  const bool m_dump_pseudo;
};

/* Dump from reg_buffer, used when there is no thread or
   registers.  */

class register_dump_reg_buffer : public register_dump, reg_buffer
{
public:
  register_dump_reg_buffer (gdbarch *gdbarch, bool dump_pseudo,
			    bool hide_nameless)
    : register_dump (gdbarch, hide_nameless),
      reg_buffer (gdbarch, dump_pseudo)
  {
  }

protected:
  void dump_reg (ui_file *file, int regnum) override
  {
    if (regnum < 0)
      {
	if (m_has_pseudo)
	  fprintf_unfiltered (file, "Cooked value");
	else
	  fprintf_unfiltered (file, "Raw value");
      }
    else
      {
	if (regnum < gdbarch_num_regs (m_gdbarch) || m_has_pseudo)
	  {
	    auto size = register_size (m_gdbarch, regnum);

	    if (size == 0)
	      return;

	    auto status = get_register_status (regnum);

	    gdb_assert (status != REG_VALID);

	    if (status == REG_UNKNOWN)
	      fprintf_unfiltered (file, "<invalid>");
	    else
	      fprintf_unfiltered (file, "<unavailable>");
	  }
	else
	  {
	    /* Just print "<cooked>" for pseudo register when
	       regcache_dump_raw.  */
	    fprintf_unfiltered (file, "<cooked>");
	  }
      }
  }
};

/* For "maint print registers".  */

class register_dump_none : public register_dump
{
public:
  register_dump_none (gdbarch *arch, bool hide_nameless)
    : register_dump (arch, hide_nameless)
  {}

protected:
  void dump_reg (ui_file *file, int regnum) override
  {}
};

/* For "maint print remote-registers".  */

class register_dump_remote : public register_dump
{
public:
  register_dump_remote (gdbarch *arch, bool hide_nameless)
    : register_dump (arch, hide_nameless)
  {}

protected:
  void dump_reg (ui_file *file, int regnum) override
  {
    if (regnum < 0)
      {
	fprintf_unfiltered (file, "Rmt Nr  g/G Offset");
      }
    else if (regnum < gdbarch_num_regs (m_gdbarch))
      {
	int pnum, poffset;

	if (remote_register_number_and_offset (m_gdbarch, regnum,
					       &pnum, &poffset))
	  fprintf_unfiltered (file, "%7d %11d", pnum, poffset);
      }
  }
};

/* For "maint print register-groups".  */

class register_dump_groups : public register_dump
{
public:
  register_dump_groups (gdbarch *arch, bool hide_nameless)
    : register_dump (arch, hide_nameless)
  {}

protected:
  void dump_reg (ui_file *file, int regnum) override
  {
    if (regnum < 0)
      fprintf_unfiltered (file, "Groups");
    else
      {
	const char *sep = "";
	struct reggroup *group;

	for (group = reggroup_next (m_gdbarch, NULL);
	     group != NULL;
	     group = reggroup_next (m_gdbarch, group))
	  {
	    if (gdbarch_register_reggroup_p (m_gdbarch, regnum, group))
	      {
		fprintf_unfiltered (file,
				    "%s%s", sep, reggroup_name (group));
		sep = ",";
	      }
	  }
      }
  }
};

/* Structure to hold the options use by maintenance register printing
   commands.  */

struct maint_print_regs_options
{
  bool hide_nameless_registers = false;
};

/* The options used by maintenance register printing commands.  */

static const gdb::option::option_def maint_print_regs_options_defs[] = {
  /* Select the size of the x-registers.  */
  gdb::option::boolean_option_def<maint_print_regs_options> {
    "hide-nameless-registers",
    [] (maint_print_regs_options *opts) {
      return &opts->hide_nameless_registers;
    },
    nullptr, /* show_cmd_cb */
    N_("Hide registers with no name.")
  },
};

/* Create an option_def_group for the maint_print_regs_options_defs, with
   OPTS as context.  */

static inline gdb::option::option_def_group
make_maint_print_regs_options_def_group
	(maint_print_regs_options *opts)
{
  return {{maint_print_regs_options_defs}, opts};
}

/* Completer for "maint print *" register based commands.  */

static void
maint_print_regs_completer (cmd_list_element *ignore,
			    completion_tracker &tracker,
			    const char *text,
			    const char * /* word */)
{
  const auto group
    = make_maint_print_regs_options_def_group (nullptr);
  if (gdb::option::complete_options
      (tracker, &text, gdb::option::PROCESS_OPTIONS_UNKNOWN_IS_OPERAND, group))
    return;

  const char *word = advance_to_expression_complete_word_point (tracker, text);
  filename_completer (ignore, tracker, text, word);
}

enum regcache_dump_what
{
  regcache_dump_none, regcache_dump_raw,
  regcache_dump_cooked, regcache_dump_groups,
  regcache_dump_remote
};

/* Called after GDB fails to open a file in REGCACHE_PRINT, calls
   PERROR_WITH_NAME passing in an appropriate name based on WHAT_TO_DUMP.  */
static void
regcache_print_open_perror (enum regcache_dump_what what_to_dump)
{
  /* Don't internationalise the name strings passed in here as these are
     the names of commands, which are not themselves internationalised.  */
  switch (what_to_dump)
    {
    case regcache_dump_none:
      perror_with_name ("maintenance print registers");
    case regcache_dump_raw:
      perror_with_name ("maintenance print raw-registers");
    case regcache_dump_cooked:
      perror_with_name ("maintenance print cooked-registers");
    case regcache_dump_groups:
      perror_with_name ("maintenance print register-groups");
    case regcache_dump_remote:
      perror_with_name ("maintenance print remote-registers");
    };
}

static void
regcache_print (const char *args, enum regcache_dump_what what_to_dump)
{
  /* Process command arguments.  */
  maint_print_regs_options opts;
  auto group = make_maint_print_regs_options_def_group (&opts);
  gdb::option::process_options
    (&args, gdb::option::PROCESS_OPTIONS_UNKNOWN_IS_OPERAND, group);
  bool hide_nameless = opts.hide_nameless_registers;

  /* Where to send output.  */
  stdio_file file;
  ui_file *out;

  if (args == NULL || *args == '\0')
    out = gdb_stdout;
  else
    {
      if (!file.open (args, "w"))
	regcache_print_open_perror (what_to_dump);
      out = &file;
    }

  std::unique_ptr<register_dump> dump;
  std::unique_ptr<regcache> regs;
  gdbarch *gdbarch;

  if (target_has_registers)
    gdbarch = get_current_regcache ()->arch ();
  else
    gdbarch = target_gdbarch ();

  switch (what_to_dump)
    {
    case regcache_dump_none:
      dump.reset (new register_dump_none (gdbarch, hide_nameless));
      break;
    case regcache_dump_remote:
      dump.reset (new register_dump_remote (gdbarch, hide_nameless));
      break;
    case regcache_dump_groups:
      dump.reset (new register_dump_groups (gdbarch, hide_nameless));
      break;
    case regcache_dump_raw:
    case regcache_dump_cooked:
      {
	auto dump_pseudo = (what_to_dump == regcache_dump_cooked);

	if (target_has_registers)
	  dump.reset (new register_dump_regcache (get_current_regcache (),
						  dump_pseudo,
						  hide_nameless));
	else
	  {
	    /* For the benefit of "maint print registers" & co when
	       debugging an executable, allow dumping a regcache even when
	       there is no thread selected / no registers.  */
	    dump.reset (new register_dump_reg_buffer (target_gdbarch (),
						      dump_pseudo,
						      hide_nameless));
	  }
      }
      break;
    }

  dump->dump (out);
}

static void
maintenance_print_registers (const char *args, int from_tty)
{
  regcache_print (args, regcache_dump_none);
}

static void
maintenance_print_raw_registers (const char *args, int from_tty)
{
  regcache_print (args, regcache_dump_raw);
}

static void
maintenance_print_cooked_registers (const char *args, int from_tty)
{
  regcache_print (args, regcache_dump_cooked);
}

static void
maintenance_print_register_groups (const char *args, int from_tty)
{
  regcache_print (args, regcache_dump_groups);
}

static void
maintenance_print_remote_registers (const char *args, int from_tty)
{
  regcache_print (args, regcache_dump_remote);
}

void _initialize_regcache_dump ();
void
_initialize_regcache_dump ()
{
  cmd_list_element *cmd;
  const auto maint_reg_print_opts
    = make_maint_print_regs_options_def_group (nullptr);

  static std::string registers_help
    = gdb::option::build_help (_("\
Print the internal register configuration.\n\
Usage: maintenance print registers [OPTIONS] [FILENAME]\n\
\n\
Options:\n\
%OPTIONS%\n\
When optional FILENAME is provided output is written to the specified\n\
file."), maint_reg_print_opts);
  cmd = add_cmd ("registers", class_maintenance, maintenance_print_registers,
		 registers_help.c_str (), &maintenanceprintlist);
  set_cmd_completer_handle_brkchars (cmd, maint_print_regs_completer);

  static std::string raw_registers_help
    = gdb::option::build_help (_("\
Print the internal register configuration including raw values.\n\
Usage: maintenance print raw-registers [OPTIONS] [FILENAME]\n\
\n\
Options:\n\
%OPTIONS%\n\
When optional FILENAME is provided output is written to the specified\n\
file."), maint_reg_print_opts);
  cmd = add_cmd ("raw-registers", class_maintenance,
		 maintenance_print_raw_registers,
		 raw_registers_help.c_str (), &maintenanceprintlist);
  set_cmd_completer_handle_brkchars (cmd, maint_print_regs_completer);

  static std::string cooked_registers_help
    = gdb::option::build_help (_("\
Print the internal register configuration including cooked values.\n\
Usage: maintenance print cooked-registers [OPTIONS] [FILENAME]\n\
\n\
Options:\n\
%OPTIONS%\n\
When optional FILENAME is provided output is written to the specified\n\
file."), maint_reg_print_opts);
  cmd = add_cmd ("cooked-registers", class_maintenance,
		 maintenance_print_cooked_registers,
		 cooked_registers_help.c_str (), &maintenanceprintlist);
  set_cmd_completer_handle_brkchars (cmd, maint_print_regs_completer);

  static std::string register_groups_help
    = gdb::option::build_help (_("\
Print the internal register configuration each registers groups.\n\
Usage: maintenance print register-groups [OPTIONS] [FILENAME]\n\
\n\
Options:\n\
%OPTIONS%\n\
When optional FILENAME is provided output is written to the specified\n\
file."), maint_reg_print_opts);
  cmd = add_cmd ("register-groups", class_maintenance,
		 maintenance_print_register_groups,
		 register_groups_help.c_str (), &maintenanceprintlist);
  set_cmd_completer_handle_brkchars (cmd, maint_print_regs_completer);

  static std::string remote_registers_help
    = gdb::option::build_help (_("\
Print the internal register configuration including remote register\n\
number and g/G packets offset.\n\
Usage: maintenance print remote-registers [OPTIONS] [FILENAME]\n\
\n\
Options:\n\
%OPTIONS%\n\
When optional FILENAME is provided output is written to the specified\n\
file."), maint_reg_print_opts);
  cmd = add_cmd ("remote-registers", class_maintenance,
		 maintenance_print_remote_registers,
		 remote_registers_help.c_str (), &maintenanceprintlist);
  set_cmd_completer_handle_brkchars (cmd, maint_print_regs_completer);
}
