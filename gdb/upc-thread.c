/* UPC language thread support for GDB, the GNU debugger.

   Contributed by: Nenad Vukicevic <nenad@intrepid.com>

   Copyright (C) 2008, 2009 Free Software Foundation, Inc.

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

#include <signal.h>
#include <wait.h>
#include "defs.h"
#include "command.h"
#include "gdbcmd.h"
#include "gdbcore.h"
#include "target.h"
#include "observer.h"
#include <sys/procfs.h>
#include "gregset.h"
#include "regcache.h"
#include "inferior.h"
#include "gdbthread.h"
#include "top.h"
#include "upc-thread.h"

#define	GDB_UPC_PROMPT "(gdb-upc) "

/* Print debugging traces if set to non-zero.  */
static int debug_upc_thread = 0;

/* Non-zero if the upc-thread layer is active.  */
int upc_thread_active = 0;
/* Non-zero if pthreads implementation of run-time */
int upc_pthread_active = 0;

/* Indicate upc-start command */
static int upc_start_in_progress = 0;

/* The upc-thread target_ops structure.  */
static struct target_ops upc_thread_ops;

extern struct thread_info *thread_list;
extern int highest_thread_num;
extern int upc_lang_initialized;

struct thread_info *upc_thread0 = NULL;
struct thread_info *upc_monitor = NULL;
struct inferior *upc_monitor_inferior = NULL;
int upc_monitor_pid = 0;
int upc_threads = 0;		/* number of static/dynamic UPC threads (compiled or requested) */
int upc_thread_cnt = 0;		/* number of UPC threads attached */
int upc_thread_exit_cnt = 0;	/* number of UPC threads that exited */
int upc_exit_code = 0; /* UPC program exit code - in case of upc_global_exit() we remember the exit code */
int upc_sync_ok = 0; /* OK to use upc-sync command */

/* Enable/Disable UPC threads sync on startup
    = 0, threads run freely on startup
    = 1, threads are waiting for GDB to lift a debug gate
*/
int upcstartgate = 1;

/* UPC debuggig mode.
   In 'upcmode' some commands that work on all
   threads will work on upc threads only.
 */
int upcmode = 0;

/* UPC standalone mode
    = 0, multi-thread/multi-process support
    = 1, single process support
*/
int upcsingle = 0;

/* Commands with a prefix of `thread'.  */
struct cmd_list_element *upc_thread_cmd_list = NULL;

/* external functions */
extern void prune_threads ();
extern void upc_lang_init (char *cmd, int from_tty);

static void upc_thread_attach (struct thread_info *t);

/* Print a debug trace if debug_upc_thread is set (its value is adjusted
   by the user using "set debug upc-thread ...").  */

static void
debug (char *format, ...)
{
  if (debug_upc_thread)
    {
      va_list args;

      va_start (args, format);
      printf_unfiltered ("UPC Threads: ");
      vprintf_unfiltered (format, args);
      printf_unfiltered ("\n");
      va_end (args);
    }
}

/* Return true if TP is an active thread. */
static int
upc_thread_alive (struct thread_info *tp)
{
  if (tp->state == THREAD_EXITED)
    return 0;
  if (!target_thread_alive (tp->ptid))
    return 0;
  return 1;
}

/* Number of UPC threads in the system */
int
upc_thread_count ()
{
  struct symbol *threads_sym;
  struct value *threads_val;
  
  if (upc_threads != 0)
    return upc_threads;
    
  /* UPC program? */
  threads_sym = lookup_symbol ("THREADS", 0, VAR_DOMAIN, NULL);
  if (threads_sym)
    {
      threads_val = read_var_value (threads_sym, NULL);
      if (threads_val)
	upc_threads = value_as_long (threads_val);
    }
  else
    {
      error (_("upc_lang_init: Can't find THREADS variable. Is this a UPC program?"));
    }

  return upc_threads;
}

/* Return TRUE if this is a valid upc thread */
int
is_upc_thread (struct thread_info *tp)
{
  if (UPC_THR_MON (tp))
    return 0;
  return 1;
}

/* Return thread number for given thread */
int
upc_thread_num (struct thread_info *tp)
{
  if (upcmode)
    return tp->unum;
  else
    return tp->num;
}

/* Return thread number for given inferior */
int
upc_thread_of_inferior (struct inferior *inf)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    {
      if (ptid_get_pid (tp->ptid) == inf->pid)
	{
	  if (upcmode)
	    return tp->unum;
	  else
	    return inf->num;
	}
    }
  return 0;
}

/* Return TRUE if this is a valid upc thread ID */
int
valid_upc_thread_id (int num)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    if (tp->unum == num)
      return 1;
  return 0;
}

/* Return GDB's thread ID for user's thread ID input */
int
valid_input_thread_id (int num)
{
  struct thread_info *tp;

  for (tp = thread_list; tp; tp = tp->next)
    {
      if (upcmode && (tp->unum == num))
	return tp->num;
      if (!upcmode && (tp->num == num))
	return num;
    }
  return -1;
}

/* Return thread ID to show from GDB thread ID */
int
show_thread_id (int num)
{
  struct thread_info *tp;

  if (!upcmode)
    return num;
  for (tp = thread_list; tp; tp = tp->next)
    if (tp->num == num)
      return tp->unum;
  return -1;
}

/* Read symbol from the current thread */
int
upc_read_thread_sym (char *sym_name)
{
  struct symbol *mythread_sym;
  mythread_sym = lookup_symbol (sym_name, 0, VAR_DOMAIN, NULL);
  if (mythread_sym)
    {
      struct value *mythread_val;
      struct frame_info *frame;
      if (!symbol_read_needs_frame (mythread_sym))
        frame = NULL;
      else
        frame = get_selected_frame (_("No frame selected."));
      mythread_val = read_var_value (mythread_sym, frame);
      if (mythread_val)
	return value_as_long (mythread_val);
    }
  return (-1);
}

/* Activate thread support if appropriate.  Do nothing if thread
   support is already active.  */

static void
upc_enable_thread_debug (void)
{
  struct minimal_symbol *msym;
  void *caller_context;
  int status;

  /* Enable UPC debugging if not already */
  if (!upc_thread_active)
    {
      msym = lookup_minimal_symbol ("THREADS", NULL, NULL);
      if (msym == NULL)
	{
	  debug ("upc_enable_thread_debug: No THREADS");
	  return;
	}

      if (upcsingle)
	return;

      push_target (&upc_thread_ops);
      upc_thread_active = 1;

      /* check for pthreads run-time */
      msym = lookup_minimal_symbol ("UPC_PTHREADS", NULL, NULL);
      if (msym != NULL)
	{
	  debug ("upc_enable_thread_debug: PTHREADS implementation");
          upc_pthread_active = 1;
	}
      debug ("upc_enable_thread_debug: Thread support enabled.");
    }
}

/* Load UPC thread debugging if UPC threaded application
   is loaded */

static void
upc_thread_new_objfile (struct objfile *objfile)
{
  if (objfile)
    upc_enable_thread_debug ();
}

static char *
upc_thread_pid_to_str (struct target_ops *ops, ptid_t ptid)
{
  static char *ret = NULL;
  struct thread_info *t = find_thread_ptid (ptid);

  if (t)
    {
      if (is_upc_thread (t))
	if (t->collective_bp_num)
	  ret =
	    xstrprintf (_("UPC Thread %d (cb %d)"), UPC_THR_NUM (t),
			t->collective_bp_num);
	else
	  ret = xstrprintf (_("UPC Thread %d"), UPC_THR_NUM (t));
      else if (t == upc_monitor)
	ret = xstrprintf (_("UPC MONITOR"));
      else
	ret = xstrprintf (_("Process %d"), ptid.pid);
    }
  else
    ret = xstrprintf (_("Process %d"), ptid.pid);

  return ret;
}

/* upc_thread_attach
     Called whenever new thread is being created. */
static void
upc_thread_attach (struct thread_info *t)
{
  struct symbol *upc_threads_hold;
  struct symbol *mythread_sym;
  CORE_ADDR sp;

  if (!upc_thread_active)
    return;

  /* first thread is always a monitor thread */
  if (!upc_monitor)
    {
      /* MONITOR thread */
      upc_monitor = t;
      UPC_THR_NUM (t) = UPC_MONITOR_THREAD;
      upc_monitor_inferior = current_inferior ();
      upc_monitor_pid = upc_monitor_inferior->pid;
    }
  else
    {
      /* UPC thread */
      if (!upc_thread0)
	{
	  /* first UPC thread is 0 */
	  upc_thread0 = t;
	  if (!upc_lang_initialized)
	    upc_lang_init (NULL, 0);
	  upc_threads = upc_read_thread_sym ("THREADS");
          if (upcstartgate && !upc_pthread_active &&
	      upc_threads != 1)
	    {
	      printf_filtered ("UPC Threads sync debugging is on.\n");
	      printf_filtered
	        ("Use upc-sync command to stop all threads and lift the debugging gate.\n");
	    }
	  upc_exit_code = 0;
	}
      UPC_THR_NUM (t) = upc_thread_cnt++;

      /* set debugging sync for each UPC thread */
      if (upcstartgate)
	{
	  CORE_ADDR gate_addr = 0;
	  gdb_byte buf = 1;
	  struct minimal_symbol *msym;

	  msym = lookup_minimal_symbol ("MPIR_being_debugged", NULL, NULL);
	  if (msym == NULL)
	    {
	      debug
		("upc_enable_thread_debug: No MPIR_being_debugged in UPC thread");
	      return;
	    }
	  gate_addr = SYMBOL_VALUE_ADDRESS (msym);
	  write_memory (gate_addr, &buf, 1);
	}
    }
}

/* Manage collective breakpoint condition for stop */
static void
upc_thread_breakpoint_created (struct breakpoint *bp)
{
  if (!is_collective_breakpoints ())
    return;

  /* skip specific breakpoints */
  if (bp->thread != -1)
    return;

  bp->max_threads_hit = upc_threads;
}

/* Manage UPC thread exit. Once all thread exited 
   monitor thread must exit too. */
static void
upc_thread_exit (struct thread_info *t, int silent)
{
  struct ui_out *uiout = current_uiout;
  char *upcmode_cmd;
  char *collective_cmd;
  if (!upc_thread_active)
    return;
  upcmode_cmd = xstrdup ("set upcmode off");
  collective_cmd = xstrdup ("set breakpoint collective off");
  debug (" %d exit\n", t->unum);
  if (t->unum != -1)
    {
      /* In case of pthreads exit message is already printed */
      if (!silent && !upc_pthread_active)
	ui_out_message (uiout, 0, "[UPC Thread %d exited]\n", t->unum);
      upc_thread_exit_cnt++;
      if (upc_thread_exit_cnt == upc_thread_cnt)
	{
	  struct thread_info *tp;
	  int thr_cnt = 0;
	  /* disable upcmode, collective bps */
	  execute_command (upcmode_cmd, 0);
	  execute_command (collective_cmd, 0);
	  /* Switch thread to monitor thread.
	     There should be only one thread left. */
	  for (tp = thread_list; tp; tp = tp->next)
	    thr_cnt++;
	  /* At this point we should have only one thread */
	  /* (two in case of pthreads) */
	  if (thr_cnt > (upc_pthread_active ? 2 : 1))
	    printf_filtered
	      ("ERROR: All UPC threads exited and there are %d threads still alive!",
	       thr_cnt - 1);
	  if (upc_pthread_active)
	    {
	      switch_to_thread (upc_monitor->ptid);
	    }   
	  /* Only print this message if the last thread is a UPC thread */
	  if (thr_cnt == 1)
	    {
	      if (upc_exit_code) 
	        {
	          ui_out_text (uiout, "Program exited with code ");
	          ui_out_field_fmt (uiout, "exit-code", "0%o", (unsigned int) upc_exit_code);
	          ui_out_text (uiout, ".\n");
	        }
	      else
	        ui_out_text (uiout, "Program exited normally.\n");
            }
	  /* Final exit or re-run */
	  upc_threads = 0;
	  upc_thread_cnt = 0;
	  upc_thread_exit_cnt = 0;
	}
    }
  /* Cleanup local upc variables */
  if (t == upc_monitor)
    {
       upc_monitor = NULL;
       upc_monitor_inferior = NULL;
    } 
  if (t == upc_thread0)
    upc_thread0 = NULL;
}

/* Cleanup after re-run (target kill) */
void
upc_thread_kill_cleanup ()
{

  /* ... make sure we clear upcmode */
  execute_command ("set upcmode off", 0);
  
  if (upc_pthread_active || (upc_threads == 1))
    return;

  /* Monitor iferior was detached. Do not leave it in zombie state, */
  /* Code take from linux-fork.c */ 
  if (upc_monitor_pid)
    {
      pid_t ret; 
      int status;
      do {
	kill (upc_monitor_pid, SIGKILL);
	ret = waitpid (upc_monitor_pid, &status, 0);
	/* We might get a SIGCHLD instead of an exit status.  This is
	 aggravated by the first kill above - a child has just
	 died.  MVS comment cut-and-pasted from linux-nat.  */
      } while (ret == upc_monitor_pid && WIFSTOPPED (status));
      upc_monitor_pid = 0;
    }
}

static void
init_upc_thread_ops (void)
{
  upc_thread_ops.to_shortname = "upc-threads";
  upc_thread_ops.to_longname = _("UPC threads support");
  upc_thread_ops.to_doc = _("UPC threads support");
  upc_thread_ops.to_pid_to_str = upc_thread_pid_to_str;
  upc_thread_ops.to_stratum = arch_stratum;
  upc_thread_ops.to_magic = OPS_MAGIC;
  upc_thread_ops.to_thread_address_space = NULL;
}

/* Switch upc threads.
   In cases of multiprocessing to read/write memory
   of other processes, the current thread must be switched
   to the target one and restored after. */

int
upc_thread_set (int upc_thr_num)
{
  struct thread_info *tp;
  struct thread_info *ctp = NULL;
  struct thread_info *ttp = NULL;
  /* find current and target threads */
  for (tp = thread_list; tp; tp = tp->next)
    {
      if (ptid_equal (tp->ptid, inferior_ptid))
	ctp = tp;
      if (upc_thr_num == UPC_THR_NUM (tp))
	ttp = tp;
    }
  if (!ctp || upc_thr_num == UPC_THR_NUM (ctp))
    return upc_thr_num;
  /* Use remote interface if exists */
  if (current_target.to_thread_switch)
    {
      ptid_t pid;
      pid.tid = GDB_THR_NUM (ttp);
      current_target.to_thread_switch (pid);
      return UPC_THR_NUM (ctp);
    }
  /* need to switch thread and its space */
  for (tp = thread_list; tp; tp = tp->next)
    {
      if (upc_thread_alive (tp) && (upc_thr_num == UPC_THR_NUM (tp)))
	{
	  switch_to_thread (tp->ptid);
	}
    }
  return UPC_THR_NUM (ctp);
}

void
upc_thread_restore (int upc_thr_num)
{
  struct thread_info *tp;
  struct thread_info *ctp = NULL;
  struct thread_info *ttp = NULL;
  /* find current and target threads */
  for (tp = thread_list; tp; tp = tp->next)
    {
      if (ptid_equal (tp->ptid, inferior_ptid))
	ctp = tp;
      if (upc_thr_num == UPC_THR_NUM (tp))
	ttp = tp;
    }
  if (!ctp || upc_thr_num == UPC_THR_NUM (ctp))
    return;
  /* Use remote interface if exists */
  if (current_target.to_thread_switch)
    {
      ptid_t pid;
      pid.tid = GDB_THR_NUM (ttp);
      current_target.to_thread_switch (pid);
      return;
    }
  /* need to switch thread and its space */
  for (tp = thread_list; tp; tp = tp->next)
    {
      if (upc_thread_alive (tp) && (UPC_THR_NUM (tp) == upc_thr_num))
	switch_to_thread (tp->ptid);
    }
}

/* Synchronize UPC threads.
     - stop all threads
     - lift the debugging gate
     - verify that thread numbering is correct 
 */
static void
upc_thread_sync_command (char *arg, int from_tty)
{
  struct thread_info *tp;
  char *liftdbggate_cmd, *upcmode_cmd, *collective_cmd;
  char *collective_stepping_cmd;
  int one_thread;

  /* thread layer not active? */
  if (!upc_thread_active)
    return;
  /* all of them exited? */
  if (!upc_thread_cnt)
    return;

  /* are we running yet ? */
  if (thread_count () == 0)
    {
      printf_filtered ("There are no active threads!\n");
      return;
    } 

  /* ... are we right after run command? */
  if (!upc_sync_ok)
    return;
  upc_sync_ok = 0; /* disable multiple upc-sync commands for the same session */

  /* Verify that all UPC threads started */
  /* .. only check if multiple threads running */
  if ((thread_count () != 1) &&
      (upc_threads != upc_thread_cnt))
    {
          printf_filtered
	    ("Not all UPC threads started (started %d out of %d).\n",
	      upc_thread_cnt, upc_threads);
          return;
    }

  /* Stop all threads */
  /* ... and wait for all of them to stop */
  for (tp = thread_list; tp; tp = tp->next)
    {
      switch_to_thread (tp->ptid);
      if (is_executing (tp->ptid))
	{
	  struct inferior *inferior = current_inferior ();
	  target_stop (tp->ptid);
	  if (non_stop)
	    set_stop_requested (tp->ptid, 1);
	  inferior->control.stop_soon = STOP_QUIETLY_REMOTE;

	  /* Wait for stop before proceeding.  */
	  wait_for_inferior ();
          tp->state = THREAD_STOPPED;
	}
    }

  /* Lift the debugging gate for all threads */
  liftdbggate_cmd = xstrdup ("set MPIR_debug_gate=1");
  for (tp = thread_list; tp; tp = tp->next)
    {
      if (tp->state == THREAD_STOPPED)
        {
	  switch_to_thread (tp->ptid);
          execute_command (liftdbggate_cmd, 0);
        }
    }
  xfree (liftdbggate_cmd);

  /* Turn on UPC Mode */
  upcmode_cmd = xstrdup ("set upcmode on");
  execute_command (upcmode_cmd, 0);
  xfree (upcmode_cmd);

  /* Turn on collective breakpoints and stepping */
  collective_cmd = xstrdup ("set breakpoint collective on");
  execute_command (collective_cmd, 0);
  xfree (collective_cmd);
  collective_stepping_cmd = xstrdup ("set breakpoint collective_stepping on");
  execute_command (collective_stepping_cmd, 0);
  xfree (collective_stepping_cmd);

  /* Verify UPC thread numbers */
  /* find current and target threads */
  for (tp = thread_list; tp; tp = tp->next)
    {
      int mythread;
      if (!is_upc_thread (tp))
	continue;
      switch_to_thread (tp->ptid);
      mythread = upc_read_thread_sym ("MYTHREAD");
      if (UPC_THR_NUM (tp) != mythread)
	{
	  printf_filtered ("UPC thread remapping from %d to %d.\n",
			   UPC_THR_NUM (tp), mythread);
	  UPC_THR_NUM (tp) = mythread;
	}
    }

  /* In the case of only one thread ... */
  if (thread_count () == 1)
    {
      int threads = upc_read_thread_sym ("THREADS");
      if (threads != 1)
        {
          printf_filtered
	    ("Not all UPC threads started (started %d out of %d).\n",
	      1, threads);
	}
      upc_thread0 = thread_list;
      upc_thread0->unum = 0; /* this is THREAD 0 */
      upc_monitor = NULL;
      upc_monitor_inferior = NULL;
      upc_thread_cnt = 1;
      if (!upc_lang_initialized)
	upc_lang_init (NULL, 0);
    }

  /* switch to the first UPC thread */
  switch_to_thread (upc_thread0->ptid);

  /* In the case of processes detach MONITOR inferior */
  if (!upc_pthread_active && (thread_count () != 1))
    {
      
      /* ... detach inferior */ 
      switch_to_thread (upc_monitor->ptid);
      detach_command (NULL, 0);
      switch_to_thread (upc_thread0->ptid);
      /* ... and remove it from internal lists */
      delete_inferior_1 (upc_monitor_inferior, 1);

      /* ... and remove monitor thread (silently) */
      delete_thread_silent (upc_monitor->ptid);
      upc_monitor = NULL;
    }

}

/* Show UPC mode - on or off
   In 'upcmode' some commands that work on all
   threads will work on upc threads only.
 */
static void
show_upcmode (struct ui_file *file, int from_tty,
	      struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("UPC mode for threads is %s.\n"), value);
}

/* Set UPC prompt when in upcmode */
static int upc_prompt_stack = 0;
static char *old_prompt;
static void
set_upcmode (char *args, int from_tty, struct cmd_list_element *c)
{
  struct ui_out *uiout = current_uiout;

  /* TODO: it would be nice to use push/pop_prompt. However,
     pop does not restore old prompt - see event-top.c */
  if (upcmode && !upc_prompt_stack)
    {
      old_prompt = xstrdup (get_prompt ());
      set_prompt (GDB_UPC_PROMPT);
      upc_prompt_stack++;
      ui_out_message (uiout, 0, "UPC Mode activated.\n");
    }
  else if (!upcmode && (upc_prompt_stack == 1))
    {
      set_prompt (old_prompt);
      upc_prompt_stack--;
      ui_out_message (uiout, 0, "UPC Mode de-activated.\n");
    }
  else
    {
    }
}

extern void _initialize_upc_thread (void);

static void
show_upcsingle (struct ui_file *file, int from_tty,
		    struct cmd_list_element *c, const char *value)
{
  fprintf_filtered (file, _("UPC single process mode is %s.\n"), value);
}

void
_initialize_upc_thread ()
{
  init_upc_thread_ops ();
  add_target (&upc_thread_ops);

  /* upc commands */
  /* stop and release all UPC threads on debug gate */
  add_com ("upc-sync", class_run, upc_thread_sync_command,
	   _("Sync upc threads."));

  /* symplified thread apply */
  add_com_alias ("upcall", "thread apply all", class_run, 1);
  add_com_alias ("uall", "thread apply all", class_run, 1);
  add_com_alias ("all", "thread apply all", class_run, 1);

  /* upc related variables */
  /* enable sync of UPC threads on startup */
  add_setshow_boolean_cmd ("upcstartgate", class_support, &upcstartgate, _("\
Set UPC startup sync mode."), _("\
Show UPC startup sync mode."), NULL, NULL, NULL, &setlist, &showlist);

  /* turn on/off UPC thread display mode */
  add_setshow_boolean_cmd ("upcmode", class_support, &upcmode, _("\
Set UPC mode thread comamnds."), _("\
Show UPC mode thread commands."), NULL, set_upcmode, show_upcmode, &setlist, &showlist);
  
  /* turn on/off standalone process mode */
  add_setshow_boolean_cmd ("upcsingle", class_support, &upcsingle, _("\
Set UPC single process mode."), _("\
Show UPC single process mode."), NULL, NULL, show_upcsingle, &setlist, &showlist);  

  add_setshow_boolean_cmd ("upc-threads", class_maintenance,
			   &debug_upc_thread,
			   _("Set debugging of UPC threads module."),
			   _("Show debugging of UPC threads module."),
			   _("Enables debugging output (used to debug GDB)."),
			   NULL, NULL, &setdebuglist, &showdebuglist);

  /* upc observers */
  /* add observer for obj file loading */
  observer_attach_new_objfile (upc_thread_new_objfile);

  /* add observer for thread create */
  observer_attach_new_thread (upc_thread_attach);

  /* add observer for breakpoints set */
  observer_attach_breakpoint_created (upc_thread_breakpoint_created);

  /* add observer for thread exit */
  observer_attach_thread_exit (upc_thread_exit);

}
