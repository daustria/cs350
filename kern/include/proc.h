/*
 * Copyright (c) 2013
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

#ifndef _PROC_H_
#define _PROC_H_

/*
 * Definition of a process.
 *
 * Note: curproc is defined by <current.h>.
 */

#include <spinlock.h>
#include <thread.h> /* required for struct threadarray */
#include <array.h>
#include <synch.h>
#include "opt-A2.h"

struct addrspace;
struct vnode;
#ifdef UW
struct semaphore;
#endif // UW

struct proc;

#ifdef OPT_A2

//Declare a struct children structure, which is an array
//of pointers to proc

DECLARRAY_BYTYPE(childarray, struct proc);

#ifndef CHILDINLINE
#define CHILDINLINE INLINE
#endif //CHILDINLINE

DEFARRAY_BYTYPE(childarray, struct proc, CHILDINLINE);

#endif //OPT_A2

/*
 * Process structure.
 */
struct proc {
	char *p_name;			/* Name of this process */
	struct spinlock p_lock;		/* Lock for this structure */
	struct threadarray p_threads;	/* Threads in this process */

	/* VM */
	struct addrspace *p_addrspace;	/* virtual address space */

	/* VFS */
	struct vnode *p_cwd;		/* current working directory */

#ifdef UW

  /* a vnode to refer to the console device */
  /* this is a quick-and-dirty way to get console writes working */
  /* you will probably need to change this when implementing file-related
     system calls, since each process will need to keep track of all files
     it has opened, not just the console. */

  	struct vnode *console;		/* a vnode for the console device */
#endif

	/* add more material here as needed */

#ifdef OPT_A2

	/* Keep track of the parent, so that when we exit, we can decide either to be a zombie (so our parent can read our exit code) or 
	 * just exit and kill ourselves if our parent is already dead */
  	struct proc *parent;  

	/* Keep track of our children, for waitpid (since a thread can only call waitpid on its children) */
	struct childarray *p_children;
	pid_t p_pid;


	/* These will be used for sleeping on our child when we call waitpid, waiting for 
	 * the child to become a zombie */
	struct cv *p_zombie;
	struct lock *p_zombie_mutex;

	int exitstatus;

	/* For now we'll say a zombie is a process where the threadarray and address space are claned up*/
	bool zombie; 

#endif /* OPT_A2 */
};

/* This is the process structure for the kernel and for kernel-only threads. */
extern struct proc *kproc;

/* Semaphore used to signal when there are no more processes */
#ifdef UW
extern struct semaphore *no_proc_sem;
#endif // UW

/* Call once during system startup to allocate data structures. */
void proc_bootstrap(void);

/* Create a fresh process for use by runprogram(). */
struct proc *proc_create_runprogram(const char *name);

/* Destroy a process. */
void proc_destroy(struct proc *proc);

/* Attach a thread to a process. Must not already have a process. */
int proc_addthread(struct proc *proc, struct thread *t);

/* Detach a thread from its process. */
void proc_remthread(struct thread *t);

/* Fetch the address space of the current process. */
struct addrspace *curproc_getas(void);

/* Change the address space of the current process, and return the old one. */
struct addrspace *curproc_setas(struct addrspace *);

#ifdef OPT_A2

/* Remove child from the childarray of parent */
void proc_removechild(struct proc *parent, struct proc *child);


/* Add child as a child process of parent. Return retval of array_addchild (0 if successful).
 * Made for use inside sys_fork */
int proc_addchild(struct proc *parent, struct proc *child);

/* Fetch the child process whose pid matches childid. If none exist, return NULL */
struct proc *proc_getchild(struct proc *proc, pid_t childid);

/* Delete zombie children. To be used in sys__exit() (when becoming a zombie) */
void proc_destroy_zombie_children(struct proc *proc);

#endif //OPT_A2

#endif /* _PROC_H_ */
