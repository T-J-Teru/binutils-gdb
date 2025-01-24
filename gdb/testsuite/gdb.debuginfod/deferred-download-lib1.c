/* This testcase is part of GDB, the GNU debugger.

   Copyright 2023-2026 Free Software Foundation, Inc.

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

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

extern void libdeferred2_test ();
extern void *libdeferred2_thread_test (void *);

static volatile int flag = 0;

void
libdeferred1_test ()
{
  pthread_t thr;

  printf ("In libdeferred1\n");
  libdeferred2_test ();

  pthread_create (&thr, NULL, libdeferred2_thread_test, (void *) &flag);

  /* Give the new thread a chance to actually enter libdeferred2_thread_test.  */
  while (!flag)
    ;

  printf ("Cancelling thread\n");
  pthread_cancel (thr);
}
