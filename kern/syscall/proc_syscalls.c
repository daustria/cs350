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
//Mutual exclusion for pid assignment in sys_fork()
extern volatile pid_t pid_counter;
extern struct spinlock pid_counter_mutex;
#endif //OPT_A2

#ifdef OPT_A2

void sys__exit(int exitcode)
{
	struct addrspace *as;
	struct proc *p = curproc;

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


#else

/* this implementation of sys__exit does not do anything with the exit code */
/* this needs to be fixed to get exit() and waitpid() working properly */
void sys__exit(int exitcode) {

	struct addrspace *as;
	struct proc *p = curproc;

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
#endif //OPT_A2

#ifdef OPT_A2
	int
sys_getpid(pid_t *retval)
{
	//Question: Can this ever fail?
	if(curproc == NULL)
	{
		return ESRCH;
	}

	*retval = curproc->p_pid;
	return 0;
}
#else
/* stub handler for getpid() system call                */
	int
sys_getpid(pid_t *retval)
{
	/* for now, this is just a stub that always returns a PID of 1 */
	/* you need to fix this to make it work properly */
	*retval = 1;
	return(0);
}
#endif //OPT_A2


#ifdef OPT_A2
int sys_waitpid(pid_t pid, userptr_t status, int options, pid_t *retval)
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


	//Case 1: The child does not exist.

	//Case 2: The child exists but is not running; it is a zombie.

	//Case 3: The child exists and is running. Need to go to sleep.

	/* for now, just pretend the exitstatus is 0 */
	exitstatus = 0;


	//copies a block of sizeof(int) from the kernel address &exitstatus to
	//the user address status
	//
	//We should be careful about doing this... so there is a whole function for it.
	result = copyout((void *)&exitstatus,status,sizeof(int));

	if (result) {
		return(result);
	}
	*retval = pid;
	return(0);
}
#else
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
#endif //OPT_A2

#ifdef OPT_A2


//////////////////////////////////////////////////////////////
// sys_fork

volatile pid_t pid_counter;
struct spinlock *pid_counter_mutex_p;

int sys_fork(struct trapframe *tf, pid_t *retval)
{
	struct proc *p = curproc;

	//Assign the child in the beginning, so we can use it in the name.
	//Can help for debugging purposes.

	spinlock_acquire(&pid_counter_mutex);
	++pid_counter;
	pid_t child_pid = pid_counter;
	spinlock_release(&pid_counter_mutex);


	//TODO: Make sure child_name works out
	//
	//I want the child name to appear as {parent_name}-{child_pid}

	int length = snprintf(NULL, 0, "%d", child_pid);

	char pid_str[length + 1];

	snprintf(pid_str, length, "%d", child_pid);


	char *child_name = kmalloc((strlen(p->p_name) + length + 1) * sizeof(char));

	if (child_name == NULL) {
		return ENOMEM;
	}

	strcpy(child_name, p->p_name);
	strcat(child_name, pid_str);

	struct proc *child = proc_create_runprogram(child_name);

	if (child == NULL) {
		return ENOMEM;
	}


	struct addrspace *current_as = curproc_getas();

	struct addrspace *new_as;

	int rc = as_copy(current_as, &new_as);

	if (rc != 0)
	{
		//Kill the child process (which has no threads).
		proc_destroy(child);

		return EADDRNOTAVAIL;
	}

	//now associate the child's address space with the new
	//and initialize other fields too.

	child->p_addrspace = new_as;
	child->parent = curproc;


	// we need to assign the child a new PID.
	// (In real life, we do need reusable PIDs).		

	child->p_pid = child_pid;

	struct trapframe *tf_copy = kmalloc(sizeof(struct trapframe)); //This will be kfree-ed in the child

	if(tf_copy == NULL)
	{
		proc_destroy(child);
		return ENOMEM;
	}

	*tf_copy = *tf;

	//Put in the child_pid for debugging
	rc = thread_fork(curthread->t_name, child, enter_forked_process, (void *) tf_copy, (unsigned long) child_pid);

	if(rc != 0)
	{
		kfree(tf_copy);
		proc_destroy(child);
		return rc;
	}

	//set retval to child_pid and return 0.
	//syscall will handle the trapframe registers
	*retval = child_pid;
	return 0;


}

#endif /* OPT_A2 */
