#include <types.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/wait.h>
#include <lib.h>
#include <syscall.h>
#include <current.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>
#include <copyinout.h>
#include <mips/trapframe.h>
#include "opt-A2.h"

#ifdef OPT_A2
extern volatile pid_t pid_counter;
extern struct spinlock pid_counter_mutex;
#endif //OPT_A2

  /* this implementation of sys__exit does not do anything with the exit code */
  /* this needs to be fixed to get exit() and waitpid() working properly */

void sys__exit(int exitcode) {

  struct addrspace *as;
  struct proc *p = curproc;

  /*
   * Idea: What if I make this process a zombie, instead of exiting?
   * Then I can put the exit code somewhere in this process structure,
   * but delete all its threads.
   *
   * But then how do I know when it's safe to completely clean the process,
   * ie. kill the process in zombie state?
   *
   * Parent makes child. 
   *
   * Child is marked for becoming a zombie before dying.
   *
   * Child finishes his job, becomes zombie.
   *
   * Parent checks its code, and then kills it.
   *
   *
   * Question: What if the parent does not care when the child dies?
   * This forces the parent to always call waitPID to ensure the child is okay to die
   */

  /*
   * Idea 2: Make every process have a parent pointer, struct proc *parent.
   *
   * If the thread has a parent, then make it a zombie.
   *
   * When do we clean it? When its parent dies.
   *
   * Question: How many zombies can you accumulate this way? Is it a problem if you make
   * too many zombies?
   *
   * I like Idea 2 better, so in the implementation of fork() I will include a pointer for
   * the parent.
   */

  /* for now, just include this to keep the compiler from complaining about
     an unused variable */

  (void)exitcode;

  DEBUG(DB_SYSCALL,"Syscall: _exit(%d)\n",exitcode);

  KASSERT(curproc->p_addrspace != NULL);
  as_deactivate();
  /*
   * clear p_addrspace before calling as_destroy. Otherwise if
   * as_destroy sleeps (which is quite possible) when we
   * come back we'll be calling as_activate on a
   * half-destroyed address space. This tends to be
   * messily fatal.
   */
  as = curproc_setas(NULL);
  as_destroy(as);

  /* detach this thread from its process */
  /* note: curproc cannot be used after this call */

  //Question: Do we know that the process has only a single thread left, 
  //indicated by curthread?
  proc_remthread(curthread);

  /* if this is the last user process in the system, proc_destroy()
     will wake up the kernel menu thread */
  proc_destroy(p);
  
  thread_exit();
  /* thread_exit() does not return, so we should never get here */
  panic("return from thread_exit in sys_exit\n");
}


/* stub handler for getpid() system call                */
int
sys_getpid(pid_t *retval)
{
  /* for now, this is just a stub that always returns a PID of 1 */
  /* you need to fix this to make it work properly */
  *retval = 1;
  return(0);
}

/* stub handler for waitpid() system call                */

int
sys_waitpid(pid_t pid,
	    userptr_t status,
	    int options,
	    pid_t *retval)
{
  int exitstatus;
  int result;

  /* this is just a stub implementation that always reports an
     exit status of 0, regardless of the actual exit status of
     the specified process.   
     In fact, this will return 0 even if the specified process
     is still running, and even if it never existed in the first place.

     Fix this!
  */

  if (options != 0) {
    return(EINVAL);
  }
  /* for now, just pretend the exitstatus is 0 */
  exitstatus = 0;
  result = copyout((void *)&exitstatus,status,sizeof(int));
  if (result) {
    return(result);
  }
  *retval = pid;
  return(0);
}

#ifdef OPT_A2


//////////////////////////////////////////////////////////////
// sys_fork

volatile pid_t pid_counter;
struct spinlock *pid_counter_mutex_p;

int sys_fork(struct trapframe *tf)
{
	struct proc *p = curproc;

	//pid_t parent_pid = p->p_pid; //save the parent PID for the return values later

	//what is the name? it should be identical. so lets just make the names identical.
	//maybe something like parentName + "-child" would be better, but whatever.
	//
	//besides, we expect later that the child will call exec, which will change the name
	struct proc *child = proc_create_runprogram(p->p_name);


	if(child == NULL) {
		// let the user program know that the call failed.
		// dont panic.

		/*
		tf->tf_a3 = 1; //Failure code.
		tf->tf_v0 = ENOMEM; //Error code. No memory to make new process.
		*/

		return ENOMEM;

	} else {

		// the child was successfully created.
		//
		// we now need to make a thread for the child, and make it an 
		// exact clone of its parent process excepting the return code
		// from sys_fork(), which, instead of 0, should be the PID of its parent.
		//
		// use as_copy() to give it a copy of the parent address space.
		
		struct addrspace *current_as = curproc_getas();

		int rc = as_copy(current_as, &child->p_addrspace);

		if(rc != 0)
		{
			/*
			//Set up trap frame 
			tf->tf_a3 = 1;
			tf->tf_v0 = EADDRNOTAVAIL;
			*/

			//Kill the child process (which has no threads).
			proc_destroy(child);

			return EADDRNOTAVAIL;
		}


		////////////////////////////////////////////////////////////////////
		// PID ASSIGNMENT

		//we need to assign the child a new PID.
		// (In real life, we do need reusable PIDs).		
		
		spinlock_acquire(&pid_counter_mutex);
		pid_t child_pid = pid_counter;
		++pid_counter;
		spinlock_release(&pid_counter_mutex);

		tf->tf_a3 = 0; //success code
		tf->tf_v0 = child_pid; //return value from fork

		/*
		 * Next step: Create a new thread for the child process.
		 * We'll use thread_fork().
		 * 
		 * To do this, we'll use enter_forked_process() helper function.
		 * 
		 * enter_forked_process will take 0 as its second parameter for now, I'm not
		 * sure what it should be. maybe we'll know when we get around to writing it.	
		 */
	
		thread_fork(curthread->t_name, child, enter_forked_process, tf, 0);

		return 0;

	}

}

#endif /* OPT_A2 */
