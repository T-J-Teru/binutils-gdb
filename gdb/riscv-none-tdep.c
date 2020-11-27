/* Copyright (C) 2020 Free Software Foundation, Inc.

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

/* This file contain code that is specific for bare-metal RISC-V targets.  */

#include "defs.h"
#include "inferior.h"
#include "gdbcore.h"
#include "target.h"
#include "arch-utils.h"
#include "regcache.h"
#include "osabi.h"
#include "riscv-tdep.h"
#include "elf-bfd.h"
#include "regset.h"
#include "user-regs.h"
#include "target-descriptions.h"

/* Function declarations.  */

static void riscv_collect_regset_section_cb
	(const char *sect_name, int supply_size, int collect_size,
	 const struct regset *regset, const char *human_name, void *cb_data);

/* Function Definitions.  */

/* Called to figure out a target description for the corefile being read.
   If we get here then the corefile didn't have a target description
   embedded inside it, so we need to figure out a default description
   based just on the properties of the corefile itself.  */

static const struct target_desc *
riscv_core_read_description (struct gdbarch *gdbarch,
			     struct target_ops *target,
			     bfd *abfd)
{
  error (_("unable to figure out target description for RISC-V core files"));
  return nullptr;
}

/* Determine which signal stopped execution.  */

static int
find_signalled_thread (struct thread_info *info, void *data)
{
  if (info->suspend.stop_signal != GDB_SIGNAL_0
      && info->ptid.pid () == inferior_ptid.pid ())
    return 1;

  return 0;
}

/* Structure for passing information from riscv_corefile_thread via an
   iterator to riscv_collect_regset_section_cb. */

struct riscv_collect_regset_section_cb_data
{
  riscv_collect_regset_section_cb_data
	(struct gdbarch *gdbarch, const struct regcache *regcache,
	 ptid_t ptid, bfd *obfd, enum gdb_signal stop_signal,
	 gdb::unique_xmalloc_ptr<char> *note_data, int *note_size)
	  : gdbarch (gdbarch), regcache (regcache), obfd (obfd),
	    note_data (note_data), note_size (note_size),
	    stop_signal (stop_signal)
  {
    /* The LWP is often not available for bare metal target, in which case
       use the tid instead.  */
    if (ptid.lwp_p ())
      lwp = ptid.lwp ();
    else
      lwp = ptid.tid ();
  }

  struct gdbarch *gdbarch;
  const struct regcache *regcache;
  bfd *obfd;
  gdb::unique_xmalloc_ptr<char> *note_data;
  int *note_size;
  long lwp;
  enum gdb_signal stop_signal;
  bool abort_iteration = false;
};

/* Records information about the single thread INFO into *NOTE_DATA, and
   updates *NOTE_SIZE.  OBFD is the core file being generated.  GDBARCH is
   the architecture the core file is being created for.  */

static void
riscv_corefile_thread (struct gdbarch *gdbarch, bfd *obfd,
		       struct thread_info *info,
		       gdb::unique_xmalloc_ptr<char> *note_data,
		       int *note_size)
{
  struct regcache *regcache
    = get_thread_arch_regcache (info->inf->process_target (),
				info->ptid, gdbarch);

  /* Ideally we should be able to read all of the registers known to this
     target.  Unfortunately, sometimes targets advertise CSRs that can't be
     read.  We don't want these registers to prevent a core file being
     dumped, so we fetch the registers one by one here, and ignore any
     errors.  This does mean that the register will show up as zero in the
     core dump, which might be confusing, but probably better than being
     unable to dump a core file.  */
  for (int regnum = 0; regnum < gdbarch_num_regs (gdbarch); ++regnum)
    {
      try
        {
          target_fetch_registers (regcache, regnum);
        }
      catch (const gdb_exception_error &e)
        { /* Ignore errors.  */ }
    }

  /* Call riscv_collect_regset_section_cb for each regset, passing in the
     DATA object.  Appends the core file notes to *(DATA->NOTE_DATA) to
     describe all the registers in this thread.  */
  riscv_collect_regset_section_cb_data data (gdbarch, regcache, info->ptid,
					     obfd, info->suspend.stop_signal,
					     note_data, note_size);
  gdbarch_iterate_over_regset_sections (gdbarch,
					riscv_collect_regset_section_cb,
					&data, regcache);
}

/* Build the note section for a corefile, and return it in a malloc
   buffer.  Currently this just  dumps all available registers for each
   thread.  */

static gdb::unique_xmalloc_ptr<char>
riscv_make_corefile_notes (struct gdbarch *gdbarch, bfd *obfd, int *note_size)
{
  if (!gdbarch_iterate_over_regset_sections_p (gdbarch))
    return NULL;

  /* Data structures into which we accumulate the core file notes.  */
  gdb::unique_xmalloc_ptr<char> note_data;

  /* Add note information about the executable and its arguments.  */
  char fname[16] = {'\0'};
  char psargs[80] = {'\0'};
  if (get_exec_file (0))
    {
      strncpy (fname, lbasename (get_exec_file (0)), sizeof (fname));
      fname[sizeof (fname) - 1] = 0;
      strncpy (psargs, get_exec_file (0), sizeof (psargs));
      psargs[sizeof (psargs) - 1] = 0;

      const char *inf_args = get_inferior_args ();
      if (inf_args != nullptr && *inf_args != '\0'
	  && (strlen (inf_args)
	      < ((int) sizeof (psargs) - (int) strlen (psargs))))
	{
	  strncat (psargs, " ",
		   sizeof (psargs) - strlen (psargs));
	  strncat (psargs, inf_args,
		   sizeof (psargs) - strlen (psargs));
	}

      psargs[sizeof (psargs) - 1] = 0;
    }
  note_data.reset (elfcore_write_prpsinfo (obfd, note_data.release (),
					   note_size, fname,
					   psargs));

  /* Update our understanding of the available threads.  */
  try
    {
      update_thread_list ();
    }
  catch (const gdb_exception_error &e)
    {
      exception_print (gdb_stderr, e);
    }

  /* As we do in linux-tdep.c, prefer dumping the signalled thread first.
     The "first thread" is what tools use to infer the signalled thread.
     In case there's more than one signalled thread, prefer the current
     thread, if it is signalled.  */
  thread_info *signalled_thr, *curr_thr = inferior_thread ();
  if (curr_thr->suspend.stop_signal != GDB_SIGNAL_0)
    signalled_thr = curr_thr;
  else
    {
      signalled_thr = iterate_over_threads (find_signalled_thread, nullptr);
      if (signalled_thr == nullptr)
	signalled_thr = curr_thr;
    }

  /* First add information about the signalled thread, then add information
     about all the other threads, see above for the reasoning.  */
  riscv_corefile_thread (gdbarch, obfd, signalled_thr, &note_data, note_size);
  for (thread_info *thr : current_inferior ()->non_exited_threads ())
    {
      if (thr == signalled_thr)
	continue;
      riscv_corefile_thread (gdbarch, obfd, signalled_thr, &note_data,
			     note_size);
    }

  return note_data;
}

/* Define the general register mapping.  This follows the same format as
   the RISC-V linux corefile.  The linux kernel puts the PC at offset 0,
   gdb puts it at offset 32.  Register x0 is always 0 and can be ignored.
   Registers x1 to x31 are in the same place.  */

static const struct regcache_map_entry riscv_gregmap[] =
{
  { 1,  RISCV_PC_REGNUM, 0 },
  { 31, RISCV_RA_REGNUM, 0 }, /* x1 to x31 */
  { 0 }
};

/* Define the FP register mapping.  This follows the same format as the
   RISC-V linux corefile.  The kernel puts the 32 FP regs first, and then
   FCSR.  */

static const struct regcache_map_entry riscv_fregmap[] =
{
  { 32, RISCV_FIRST_FP_REGNUM, 0 },
  { 1, RISCV_CSR_FCSR_REGNUM, 0 },
  { 0 }
};

/* Define the general register regset.  */

static const struct regset riscv_gregset =
{
  riscv_gregmap, regcache_supply_regset, regcache_collect_regset
};

/* Define the FP register regset.  */

static const struct regset riscv_fregset =
{
  riscv_fregmap, regcache_supply_regset, regcache_collect_regset
};

/* Define the CSR regset, this is not constant as the regmap field is
   updated dynamically based on the current target description.  */

static struct regset riscv_csrset =
{
  nullptr, regcache_supply_regset, regcache_collect_regset
};

/* Update the regmap field of RISCV_CSRSET based on the CSRs available in
   the current target description.  */

static void
riscv_update_csrmap (struct gdbarch *gdbarch,
		     const struct tdesc_feature *feature_csr)
{
  int i = 0;

  /* Release any previously defined map.  */
  delete[] ((struct regcache_map_entry *) riscv_csrset.regmap);

  /* Now create a register map for every csr found in the target
     description.  */
  struct regcache_map_entry *riscv_csrmap
    = new struct regcache_map_entry[feature_csr->registers.size() + 1];
  for (auto &csr : feature_csr->registers)
    {
      int regnum = user_reg_map_name_to_regnum (gdbarch, csr->name.c_str(),
						csr->name.length());
      riscv_csrmap[i++] = {1, regnum, 0};
    }

  /* Mark the end of the array.  */
  riscv_csrmap[i] = {0};
  riscv_csrset.regmap = riscv_csrmap;
}

/* Callback for iterate_over_regset_sections that records a single regset
   in the corefile note section.  */

static void
riscv_collect_regset_section_cb (const char *sect_name, int supply_size,
				 int collect_size, const struct regset *regset,
				 const char *human_name, void *cb_data)
{
  riscv_collect_regset_section_cb_data *data
    = (riscv_collect_regset_section_cb_data *) cb_data;

  /* The only flag is REGSET_VARIABLE_SIZE, and we don't use that.  */
  gdb_assert (regset->flags == 0);
  gdb_assert (supply_size == collect_size);

  if (data->abort_iteration)
    return;

  gdb_assert (regset != nullptr && regset->collect_regset);

  /* This is intentionally zero-initialized by using std::vector, so that
     any padding bytes in the core file will show a zero.  */
  std::vector<gdb_byte> buf (collect_size);

  regset->collect_regset (regset, data->regcache, -1, buf.data (),
			  collect_size);

  /* PRSTATUS still needs to be treated specially.  */
  if (strcmp (sect_name, ".reg") == 0)
    data->note_data->reset (elfcore_write_prstatus
			    (data->obfd, data->note_data->release (),
			     data->note_size, data->lwp,
			     gdb_signal_to_host (data->stop_signal),
			     buf.data ()));
  else
    data->note_data->reset (elfcore_write_register_note
			    (data->obfd, data->note_data->release (),
			     data->note_size,
			     sect_name, buf.data (), collect_size));

  if (data->note_data->get () == NULL)
    data->abort_iteration = true;
}

/* Implement the "iterate_over_regset_sections" gdbarch method.  */

static void
riscv_iterate_over_regset_sections (struct gdbarch *gdbarch,
				    iterate_over_regset_sections_cb *cb,
				    void *cb_data,
				    const struct regcache *regcache)
{
  /* Write out the GPRs.  */
  int sz = 32 * riscv_isa_xlen (gdbarch);
  cb (".reg", sz, sz, &riscv_gregset, NULL, cb_data);

  /* Write out the FPRs, but only if present.  */
  if (riscv_isa_flen (gdbarch) > 0)
    {
      sz = (32 * riscv_isa_flen (gdbarch)
	    + register_size (gdbarch, RISCV_CSR_FCSR_REGNUM));
      cb (".reg2", sz, sz, &riscv_fregset, NULL, cb_data);
    }

  /* Read or write the CSRs.  The set of CSRs is defined by the current
     target description.  The user is responsible for ensuring that the
     same target description is in use when reading the core file as was
     in use when writing the core file.  */
  const struct target_desc *tdesc = gdbarch_target_desc (gdbarch);

  /* Do not dump/load any CSRs if there is no target description or the target
     description does not contain any CSRs.  */
  if (tdesc != nullptr)
    {
      const struct tdesc_feature *feature_csr
        = tdesc_find_feature (tdesc, riscv_feature_name_csr);
      if (feature_csr != nullptr && feature_csr->registers.size () > 0)
	{
	  riscv_update_csrmap (gdbarch, feature_csr);
	  cb (".reg-riscv-csr",
	      (feature_csr->registers.size() * riscv_isa_xlen (gdbarch)),
	      (feature_csr->registers.size() * riscv_isa_xlen (gdbarch)),
	      &riscv_csrset, NULL, cb_data);
	}
    }
}

/* Initialize RISC-V bare-metal ABI info.  */

static void
riscv_none_init_abi (struct gdbarch_info info, struct gdbarch *gdbarch)
{
  /* Find or create a target description from a core file.  */
  set_gdbarch_core_read_description (gdbarch, riscv_core_read_description);

  /* How to create a core file for bare metal RISC-V.  */
  set_gdbarch_make_corefile_notes (gdbarch, riscv_make_corefile_notes);

  /* Iterate over registers for reading and writing bare metal RISC-V core
     files.  */
  set_gdbarch_iterate_over_regset_sections
    (gdbarch, riscv_iterate_over_regset_sections);

}

/* Initialize RISC-V bare-metal target support.  */

void _initialize_riscv_none_tdep ();
void
_initialize_riscv_none_tdep ()
{
  gdbarch_register_osabi (bfd_arch_riscv, 0, GDB_OSABI_NONE,
			  riscv_none_init_abi);
}

