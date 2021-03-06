/* Copyright (C) 2002 Free Software Foundation, Inc.
   This file is part of the GNU C Library.
   Contributed by Ulrich Drepper <drepper@redhat.com>, 2002.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

#include <rtai_posix.h>

static pthread_mutex_t lock; // = PTHREAD_MUTEX_INITIALIZER;

static void *
tf (void *arg)
{
  pthread_t mh = (pthread_t) arg;
  void *result;

  pthread_setschedparam_np(0, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME_NP);
  if (pthread_mutex_unlock (&lock) != 0)
    {
      puts ("unlock failed");
      exit (1);
    }

  if (pthread_join (mh, &result) != 0)
    {
      puts ("join failed");
      exit (1);
    }

  if (result != (void *) 42l)
    {
      printf ("result wrong: expected %p, got %p\n", (void *) 42, result);
      exit (1);
    }

  exit (0);
}


static int
do_test (void)
{
  pthread_t th;

  if (pthread_mutex_lock (&lock) != 0)
    {
      puts ("1st lock failed");
      exit (1);
    }

  if (pthread_create (&th, NULL, tf, (void *) pthread_self ()) != 0)
    {
      puts ("create failed");
      exit (1);
    }

  if (pthread_mutex_lock (&lock) != 0)
    {
      puts ("2nd lock failed");
      exit (1);
    }

  pthread_exit ((void *) 42);
}

int main(void)
{
	pthread_setschedparam_np(0, SCHED_FIFO, 0, 0xF, PTHREAD_HARD_REAL_TIME_NP);
	start_rt_timer(0);
	pthread_mutex_init(&lock, NULL);
        do_test();
        return 0;
}
