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
#include <vfs.h>
#include <kern/fcntl.h>
#endif //OPT_A2

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

	lock_acquire(p->p_zombie_mutex);

	DEBUG(DB_SYSCALL, "sys_exit | proc:%s (pid:%d) exitcode:%d\n", p->p_name, p->p_pid, exitcode);

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

	proc_remthread(curthread);

	proc_destroy_zombie_children(p);

	/*
	 * Three cases when we can fully delete ourselves:
	 *
	 * 1. Parent already exited
	 *
	 * 2. All our children are dead (what if my parent wants to call waitpid on me?)
	 *
	 * 3. Our parent has already called waitpid on us
	 * (how would we know? ... should we set a flag?)
	 *
	 * Right now we are only handling the first case. Is it necessary to handle the second or third cases?
	 * Probably in real world, but I think the tests should pass even if we don't fully delete all our zombies
	 * or 'pass enough' at the very least
	 */
	
	/* fully delete ourselves if our parent is dead*/
	bool fully_delete = false;

	if (p->parent == NULL) {
		fully_delete = true;
		DEBUG(DB_SYSCALL,"_exit | proc:%s (pid:%d) fully deleting itself because no parent\n", p->p_name, p->p_pid); 
	} else {
		lock_acquire(p->parent->p_zombie_mutex);

		if(p->parent->zombie) {
			fully_delete = true;
			DEBUG(DB_SYSCALL,"_exit | proc:%s (pid:%d) fully deleting itself because parent is a zombie\n", p->p_name, p->p_pid); 
		}

		lock_release(p->parent->p_zombie_mutex);
	}

	if(fully_delete) {

		lock_release(p->p_zombie_mutex);
		/* if this is the last user process in the system, proc_destroy()
		   will wake up the kernel menu thread */
		proc_destroy(p);

	} else {

		DEBUG(DB_SYSCALL, "_exit | proc:%s (pid:%d) becoming a zombie instead of fully deleting, signaling parent %s (pid:%d)\n", 
				p->p_name, p->p_pid, p->parent->p_name, p->parent->p_pid);

		/* We are a zombie now so lets signal in case our parent was waiting on us */
		cv_signal(p->p_zombie_cv, p->p_zombie_mutex);

		p->exitstatus = exitcode;
		p->zombie = true;

		lock_release(p->p_zombie_mutex);
	}

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

	/* Remember, processes other than the kernel process can be assumed to have only
	 * one thread */
	proc_remthread(curthread);

	/*
	 * Now our thread and address space are freed. The true deletion will 
	 * be handled in proc_destroy.
	 */

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

	/* We are not required to implement any options */
	if (options != 0) {
		return(EINVAL);	
	}


	/* for now, just pretend the exitstatus is 0 */
	exitstatus = 0; //This is the code from exit()? What is this..

	struct proc *p = curproc;

	struct proc *child = proc_getchild(p, pid);

	if (child == NULL) {

		DEBUG(DB_SYSCALL,"sys_waitpid | ERROR: %s is not the parent of pid %d\n", p->p_name, pid);
		//The PID is not one of your children. No business calling waitpid then.
		return ECHILD;
	}

	/* Make sure the PID matches up*/
	KASSERT(child->p_pid == pid);	

	/* Wait until child becomes a zombie and then destroy it */

	/* Note: we fall asleep on the child's CV, since we are waiting on the child to become 
	 * a zombie */
	
	DEBUG(DB_SYSCALL,"sys_waitpid | proc:%s waiting on process with pid:%d\n", p->p_name, pid);

	lock_acquire(child->p_zombie_mutex);

	while(1)
	{

		DEBUG(DB_SYSCALL,"sys_waitpid | proc:%s checking if pid:%d is a zombie\n", p->p_name, pid);
		bool is_zombie = child->zombie;

		if(!is_zombie) {
			DEBUG(DB_SYSCALL,"sys_waitpid | proc:%s sleeping for %s (pid:%d) to become zombie\n", p->p_name, child->p_name, child->p_pid);
			cv_wait(child->p_zombie_cv, child->p_zombie_mutex); 
		} else {
			DEBUG(DB_SYSCALL,"sys_waitpid | proc:%s awaken since %s (pid:%d) is dead\n", p->p_name, child->p_name, child->p_pid);
			lock_release(child->p_zombie_mutex);
			break;	
		}
	}

	exitstatus = _MKWAIT_EXIT(child->exitstatus);

	DEBUG(DB_SYSCALL,"sys_waitpid | child (pid:%d) of %s exited with status %d\n", pid, p->p_name, exitstatus);

	/* We already got the exit status, so kill the zombie child now */

	//proc_removechild(p, child);

	//DEBUG(DB_SYSCALL,"sys_waitpid | %s(pid:%d) destroying child:%s(pid:%d)\n", p->p_name, p->p_pid, child->p_name, child->p_pid);

	/* Do I need to destroy the child here? When I exit, this child should be destroyed in sys__exit, when I delete all the process' zombie children */
	//proc_destroy(child);

	//copies a block of sizeof(int) from the kernel address &exitstatus to
	//the user address status
	//
	//We should be careful about doing this... so there is a whole function for it.
	result = copyout((void *)&exitstatus,status,sizeof(int));

	if (result) {
		return(result);
	}

	*retval = pid;

	return 0;
}
#else
/* stub handler for waitpid() system call */

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
	//I want the child name to appear as {parent_name}-{child_pid}

	// add 7: 6 characters from the string '-child', an extra for the null character
	char *child_name = kmalloc((strlen(p->p_name) + 7) * sizeof(char));

	if (child_name == NULL) {
		return ENOMEM;
	}

	strcpy(child_name, p->p_name);
	strcat(child_name, "-child");


	DEBUG(DB_SYSCALL,"sys_fork | pid:%d, child_name:%s (pid:%d)\n", p->p_pid, child_name, child_pid);

	struct proc *child = proc_create_runprogram(child_name);

	if (child == NULL) {
		DEBUG(DB_SYSCALL,"sys_fork | ERROR: Failed to create child (pid:%d)", child_pid);
		return ENOMEM;
	}

	/*Create a copy of the address space for the child */
	struct addrspace *current_as = curproc_getas();

	struct addrspace *new_as;

	int rc = as_copy(current_as, &new_as);

	if (rc != 0)
	{
		DEBUG(DB_SYSCALL,"sys_fork | ERROR: Failed to copy address space from %s (pid:%d) to child (pid:%d)\n", p->p_name, p->p_pid, child_pid);

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
		DEBUG(DB_SYSCALL,"sys_fork | ERROR: kmalloc failure when copying trapframe for child (pid:%d)\n", p->p_pid);
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

	/* Add the child as a child of the parent */

	rc = proc_addchild(p, child);

	if (rc != 0)
	{
		DEBUG(DB_SYSCALL," sys_fork | ERROR: Could not add %s as a child to %s\n", child->p_name, p->p_name);
		return rc;
	}

	return 0;

}

//////////////////////////////////////////////////////////////
// sys_execv

int sys_execv_count_args(userptr_t args, int *argc)
{
	int args_so_far = 0;
	int result;

	/* This cast is fine, according to piazza */
	char **kargs = (char **) args;

	while(1)
	{
		/* Copy the pointer args + args_so_far to the kernel and then check if its NULL */
		char *karg;

		/* Use copyin instead of copyinstr because we just want to check if the pointer is NULL */
		result = copyin((const_userptr_t) (kargs + args_so_far), (void *) &karg, sizeof(char *));

		if(result){
			DEBUG(DB_SYSCALL, "sys_execv_count_args | ERROR:%d could not copy the %d-th argument to the kernel\n", result, args_so_far);
			return result;
		}

		if(karg == NULL) {
			/* args is a NULL terminated array of holding elements of char *. we are done counting as soon as 
			 * we encounter a NULL pointer */

			DEBUG(DB_SYSCALL, "sys_execv_count_args | read %d arguments\n", args_so_far);

			*argc = args_so_far;
			return 0;
		} 

		++args_so_far;

	}
}

int sys_execv(userptr_t program, userptr_t args)
{
	/* To count the number of arguments */
	int argc = 0;
	int result;

	result = sys_execv_count_args(args, &argc);

	if(result) {
		return result;
	}

	/* Now that we have the number of arguments, we can copy each argument from the userspace to the kernel */

	char *kargs[argc]; 

	/* Temporarily cast to (char **) to access the underlying strings */
	char **kargs_tmp = (char **) args;

	for(int i = 0; i < argc; ++i)
	{
		/* Allocate 128 bytes for each string argument */
		kargs[i] = kmalloc(128);

		if(kargs[i] == NULL) {
			DEBUG(DB_SYSCALL, "sys_execv | ERROR:%d could not malloc when copying the %d-th argument to the kernel\n", ENOMEM, i);
			return ENOMEM;
		}

		/* Copy the string now */
		result = copyinstr((const_userptr_t) kargs_tmp[i], kargs[i], 128, NULL);

		if(result){
			DEBUG(DB_SYSCALL, "sys_execv | ERROR:%d could not copy the %d-th to the kernel as a string\n", result, i);
			return result;
		}

		DEBUG(DB_SYSCALL, "sys_execv | copied the %d-th argument:%s\n", i, kargs[i]);
	}

	/* Copy the program path from the program in the user space to the kernel */
	char kprogname[128]; 

	result = copyinstr((const_userptr_t) program, kprogname, 128, NULL);

	if(result){
		DEBUG(DB_SYSCALL, "sys_execv | ERROR: could not copy program name\n");
		return(result);
	}

	DEBUG(DB_SYSCALL, "sys_execv | copied program name:%s\n", kprogname);

	/* Copied directly from runprogram: it does these steps:
	 *
	 * 1. opens program file using vfs_open
	 * 2. creates a new address space, and set the process to the new address space.
	 * 3. load the program with load_elf
	 */

	struct addrspace *as;
	struct vnode *v;
	vaddr_t entrypoint;

	/* Open the file. */
	result = vfs_open(kprogname, O_RDONLY, 0, &v);

	if (result) {
		DEBUG(DB_SYSCALL, "sys_execv | ERROR: cannot open file %s\n", kprogname);
		return result;
	}


	struct addrspace *old_as = curproc_getas();

	if(old_as == NULL) {
		DEBUG(DB_SYSCALL, "sys_execv | ERROR: current process %s has NULL address space\n", curproc->p_name);
		panic("sys_execv | ERROR: execv called on process with no address space\n");
	}

	/* Create a new address space. */
	as = as_create();
	if (as ==NULL) {
		vfs_close(v);
		DEBUG(DB_SYSCALL, "sys_execv | ERROR: no memory to create address space for file %s\n", kprogname);
		return ENOMEM;
	}

	/* Switch to it and activate it. */
	curproc_setas(as);
	as_activate();

	/* Load the executable. */
	result = load_elf(v, &entrypoint);
	if (result) {
		DEBUG(DB_SYSCALL, "sys_execv | ERROR: could not load executable for file %s\n", kprogname);
		/* p_addrspace will go away when curproc is destroyed */
		vfs_close(v);
		return result;
	}

	/* Done with the file now. */
	vfs_close(v);

	/* Delete the old address space now */
	as_destroy(old_as);

	/* Put arguments on the user stack and get the stack ptr simultaneously */
	vaddr_t user_stack_ptr;
	userptr_t argv;

	result = as_define_stack_args(as, &argv, &user_stack_ptr, kargs, argc, kprogname);

	if(result){
		DEBUG(DB_SYSCALL, "sys_execv | ERROR:%d when copying arguments onto stack\n", result);
	}

	/* Warp to user mode */
	//enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/, user_stack_ptr, entrypoint);
	enter_new_process(argc, argv, user_stack_ptr, entrypoint);

	panic("enter_new_process returned\n");
	return EINVAL;
}

#endif /* OPT_A2 */
