/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _SYSCALL_H_
#define _SYSCALL_H_

#include <synch.h> // for lock, for the PID assignment counter.
#include "opt-A2.h"


struct trapframe; /* from <machine/trapframe.h> */

/*
 * The system call dispatcher.
 */

void syscall(struct trapframe *tf);

/*
 * Support functions.
 */

#ifdef OPT_A2
/* 
 * When making a new thread calling thread_fork(), we want to use enter_forked_process()
 * as the entry function for the new thread.
 *
 * To do this, we need to change the signature of enter_forked_process(). 
 * thread_fork() expects that the signature of the entry function:
 *
 * 1. Returns void
 * 2. Takes (void *, unsigned long) as its parameters.
 *
 * Pass the trapframe as a void * instead.
 */
void enter_forked_process(void *tf_p, unsigned long k);
#else
/* Helper for fork(). You write this. */
void enter_forked_process(struct trapframe *tf);
#endif /* OPT_A2 */

/* Enter user mode. Does not return. */
void enter_new_process(int argc, userptr_t argv, vaddr_t stackptr,
		       vaddr_t entrypoint);

/*
 * Prototypes for IN-KERNEL entry points for system call implementations.
 */

int sys_reboot(int code);
int sys___time(userptr_t user_seconds, userptr_t user_nanoseconds);

#ifdef OPT_A2

/*
 *
 * Q : For the purposes of OS161, a global PID counter will suffice for assigning
 * PID's to new processes.
 *
 * However how do we make sure the assignment is threadsafe? Use locks. But exactly how will
 *  the OS use these locks? Eg. Where will they be initialized, acquired and released? What function?
 * 
 *
 * Initializing seems to be the problem I have trouble to figure out.
 * answer: initialize in proc_bootstrap() in proc.h
 *
 */
volatile pid_t pid_counter;

//for providing mutual exclusion when updating the counter

struct spinlock pid_counter_mutex;

int sys_fork(struct trapframe *tf, pid_t *retval);
#endif /* OPT_A2 */

#ifdef UW
int sys_write(int fdesc,userptr_t ubuf,unsigned int nbytes,int *retval);
void sys__exit(int exitcode);
int sys_getpid(pid_t *retval);
int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval);

#endif // UW

#endif /* _SYSCALL_H_ */
