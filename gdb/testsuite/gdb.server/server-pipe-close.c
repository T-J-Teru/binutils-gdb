/* This testcase is part of GDB, the GNU debugger.

   Copyright 2024 Free Software Foundation, Inc.

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

#include <assert.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

/* Pipes to replace stdin, stdout, and stderr.  */

struct proc_fds
{
  int in;
  int out;
  int err;
};

/* The pipes split between parent and child process.  The child has the
   write end of the 'out' and 'err' pipes and has the read end of the 'in'
   pipe.  The parent has the other end of each pipe.  */

struct the_pipes
{
  struct proc_fds child;
  struct proc_fds parent;
};

/* Which set of pipes are we talking about?  */

enum pipe_end
{
  PARENT,
  CHILD
};

/* Initialise all the pipe file descriptors in PIPES.  Asserts if anything
   goes wrong.  */

static void
init_pipes (struct the_pipes *pipes)
{
  int res;

  /* 0: read, 1: write.  */
  int err[2];
  int out[2];
  int in[2];

  res = pipe (err);
  assert (res == 0);

  res = pipe (out);
  assert (res == 0);

  res = pipe (in);
  assert (res == 0);

  pipes->child.in = in[0];
  pipes->child.out = out[1];
  pipes->child.err = err[1];

  pipes->parent.in = in[1];
  pipes->parent.out = out[0];
  pipes->parent.err = err[0];

  /* Mark the parent's out/err read ends as non-blocking.  */
  res = fcntl(pipes->parent.err, F_SETFL,
	      fcntl(pipes->parent.err, F_GETFL) | O_NONBLOCK);
  assert (res == 0);

  res = fcntl(pipes->parent.out, F_SETFL,
	      fcntl(pipes->parent.out, F_GETFL) | O_NONBLOCK);
  assert (res == 0);
}

/* Close the relevant set of pipes in PIPES according to END.  Asserts if
   anything goes wrong.  */

static void
close_pipe_ends (struct the_pipes *pipes, enum pipe_end end)
{
  int res;
  struct proc_fds *fds;

  if (end == PARENT)
    fds = &pipes->parent;
  else
    fds = &pipes->child;

  res = close (fds->in);
  assert (res == 0);

  res = close (fds->out);
  assert (res == 0);

  res = close (fds->err);
  assert (res == 0);
}

/* Redirect current stdin, stdout, and stderr to the child ends of the
   various pipes in PIPES.  Asserts if anything goes wrong.  */

static void
redirect_child_stdio (struct the_pipes *pipes)
{
  int res;

  res = close (fileno (stdin));
  assert (res == 0);
  res = dup2 (pipes->child.in, fileno (stdin));
  assert (res == fileno (stdin));

  res = close (fileno (stdout));
  assert (res == 0);
  res = dup2 (pipes->child.out, fileno (stdout));
  assert (res == fileno (stdout));

  res = close (fileno (stderr));
  assert (res == 0);
  res = dup2 (pipes->child.err, fileno (stderr));
  assert (res == fileno (stderr));
}

/* Read a complete line from FD (up to the next newline ('\n') character),
   discard the newline character and return up to the first (LEN - 1)
   characters from the line in BUFFER.

   The string returned in BUFFER will always be NULL terminated.

   Return 0 to indicate the read was a success, otherwise raise an
   assertion if something goes wrong.  */

static int
line_from_pipe (int fd, char *buffer, size_t len)
{
  int bytes_read;
  char c;

  /* Just zero out BUFFER and reserve one character so we know there will
     always be a null terminator.  */
  memset (buffer, 0, len);
  --len;

  /* This could loop forever if something goes wrong.  Luckily the caller
     has set an alarm which will kill us if we get stuck in this loop.  */
  do
    {
      bytes_read = read (fd, &c, 1);

      /* If there's no input pending wait a short while and try again.  */
      if (bytes_read == -1 && errno == EAGAIN)
	{
	  usleep (100000);
	  continue;
	}

      /* Check the read was a success.  */
      assert (bytes_read != -1);
      assert (bytes_read == 1);

      /* End of line, discard the newline, we're done.  */
      if (c == '\n')
	break;

      /* If there's room in the buffer, add C.  */
      if (len > 0)
	{
	  *buffer = c;
	  ++buffer;
	  --len;
	}
    }
  while (1);

  return 0;
}

int
main (int argc, char *argv[])
{
  struct the_pipes all_pipes;
  int res;
  pid_t pid;

  /* Expected arguments are:
     0: filename of this program
     1: filename of gdbserver executable
     2: string '-' for gdbserver's stdio option
     3+: other gdbserver options (optional).  */
  assert (argc >= 3);

  init_pipes (&all_pipes);

  pid = fork ();
  assert (pid != -1);

  if (pid == 0)
    {
      /* Child.  */

      /* Close the parent's end of each pipe.  */
      close_pipe_ends (&all_pipes, PARENT);

      /* Redirect in/out/err to the child end of each pipe.  */
      redirect_child_stdio (&all_pipes);

      /* Start gdbserver as specified by our caller.  */
      res = execv (argv[1], &argv[1]);
      assert (res != -1);
    }
  else
    {
      /* Parent.  */
      pid_t wait_res;
      char buffer[100];

      /* Set an alarm so the test doesn't run forever.  */
      alarm (60);

      /* Close the child's end of each pipe.  */
      close_pipe_ends (&all_pipes, CHILD);

      /* Read the output from gdbserver.  This ensures that gdbserver has
	 actually started up at the point when we close our side of the
	 pipes.  */
      res = line_from_pipe (all_pipes.parent.err, buffer, sizeof (buffer));
      assert (res == 0);
      assert (strcmp (buffer, "stdin/stdout redirected") == 0);

      res = line_from_pipe (all_pipes.parent.err, buffer, sizeof (buffer));
      assert (res == 0);
      assert (strncmp (buffer, "Process ", 8) == 0);

      res = line_from_pipe (all_pipes.parent.err, buffer, sizeof (buffer));
      assert (res == 0);
      assert (strcmp (buffer, "Remote debugging using stdio") == 0);

      /* Close the stdin pipe to the child.  This should cause gdbserver
	 to exit immediately.  */
      res = close (all_pipes.parent.in);
      assert (res == 0);

      /* Wait for gdbserver to exit.  */
      wait_res = waitpid (pid, NULL, 0);
      assert (wait_res == pid);
    }

  return 0;
}
