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

/*
 * Synchronization primitives.
 * The specifications of the functions are in synch.h.
 */

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <wchan.h>
#include <thread.h>
#include <current.h>
#include <synch.h>

////////////////////////////////////////////////////////////
//
// Semaphore.

struct semaphore *
sem_create(const char *name, int initial_count)
{
        struct semaphore *sem;

        KASSERT(initial_count >= 0);

        sem = kmalloc(sizeof(struct semaphore));
        if (sem == NULL) {
                return NULL;
        }

        sem->sem_name = kstrdup(name);
        if (sem->sem_name == NULL) {
                kfree(sem);
                return NULL;
        }

	sem->sem_wchan = wchan_create(sem->sem_name);
	if (sem->sem_wchan == NULL) {
		kfree(sem->sem_name);
		kfree(sem);
		return NULL;
	}

	spinlock_init(&sem->sem_lock);
        sem->sem_count = initial_count;

        return sem;
}

void
sem_destroy(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	/* wchan_cleanup will assert if anyone's waiting on it */
	spinlock_cleanup(&sem->sem_lock);
	wchan_destroy(sem->sem_wchan);
        kfree(sem->sem_name);
        kfree(sem);
}

void 
P(struct semaphore *sem)
{
        KASSERT(sem != NULL);

        /*
         * May not block in an interrupt handler.
         *
         * For robustness, always check, even if we can actually
         * complete the P without blocking.
         */
        KASSERT(curthread->t_in_interrupt == false);

	spinlock_acquire(&sem->sem_lock);
        while (sem->sem_count == 0) {
		/*
		 * Bridge to the wchan lock, so if someone else comes
		 * along in V right this instant the wakeup can't go
		 * through on the wchan until we've finished going to
		 * sleep. Note that wchan_sleep unlocks the wchan.
		 *
		 * Note that we don't maintain strict FIFO ordering of
		 * threads going through the semaphore; that is, we
		 * might "get" it on the first try even if other
		 * threads are waiting. Apparently according to some
		 * textbooks semaphores must for some reason have
		 * strict ordering. Too bad. :-)
		 *
		 * Exercise: how would you implement strict FIFO
		 * ordering?
		 */
		wchan_lock(sem->sem_wchan);
		spinlock_release(&sem->sem_lock);
                wchan_sleep(sem->sem_wchan);

		spinlock_acquire(&sem->sem_lock);
        }
        KASSERT(sem->sem_count > 0);
        sem->sem_count--;
	spinlock_release(&sem->sem_lock);
}

void
V(struct semaphore *sem)
{
        KASSERT(sem != NULL);

	spinlock_acquire(&sem->sem_lock);

        sem->sem_count++;
        KASSERT(sem->sem_count > 0);
	wchan_wakeone(sem->sem_wchan);

	spinlock_release(&sem->sem_lock);
}

////////////////////////////////////////////////////////////
//
// Lock.

struct lock *
lock_create(const char *name)
{
        struct lock *lock;

        lock = kmalloc(sizeof(struct lock));
        if (lock == NULL) {
                return NULL;
        }

        lock->lk_name = kstrdup(name);
        if (lock->lk_name == NULL) {
                kfree(lock);
                return NULL;
        }
        
        // add stuff here as needed
	

	//initialize the name first, because like malloc and kstrdup,
	//if it fails we bail out	
	lock->wc = wchan_create(lock->lk_name);

	if (lock->wc == NULL) 
	{
		kfree(lock->lk_name);
		kfree(lock);
		return NULL;
	}

	spinlock_init(&lock->spin);


	//these fields are for who owns THIS lock, not the
	//internal spinlock in the implementation
	

	//question: why do we have held and owner? we really
	//could just have owner
	lock->held = false; //at first, nobody owns the lock
	lock->owner = NULL;

        return lock;
}

void
lock_destroy(struct lock *lock)
{
        KASSERT(lock != NULL);

        // add stuff here as needed
	
	spinlock_cleanup(&lock->spin);
	wchan_destroy(lock->wc);
 
        kfree(lock->lk_name);
        kfree(lock);
}

void
lock_acquire(struct lock *lock)
{
	KASSERT(lock != NULL);

	KASSERT(lock_do_i_hold(lock) == false); // don't want to acquire a lock we already hold.
	// otherwise, we will got sleep trying to acquire the spinlock
	// and nobody will wake us up (since nobody can relase that lock).

	//the lock is implemented as a mix of a spinlock and a wait channel
	
	//the spinlock is used for mutual exclusion.
	//
	//the wait channel is used to ensure that the spinlock is not held for too long.
	spinlock_acquire(&lock->spin);

        while (lock->held)
	{
		//lock the wait channel, so that no other thread can
		//retrieve the lock right after we release it
		wchan_lock(lock->wc);

		//release the spinlock. we are going to sleep.
		//if we have the lock while we are sleeping, then
		//nobody, including the lock owner, can wake us up. because they cant get the lock
		//
		//additionally, anyone else trying to acquire the spinlock will spin forever

		spinlock_release(&lock->spin);

                wchan_sleep(lock->wc);

		spinlock_acquire(&lock->spin);
        }

	lock->held = true;
	lock->owner = curthread;
	spinlock_release(&lock->spin);
}

void
lock_release(struct lock *lock)
{
	KASSERT(lock_do_i_hold(lock));	
	lock->owner = NULL;
	lock->held = false;

	// wake one of the sleepers. they are trying to get the lock.
	wchan_wakeone(lock->wc);

}

bool
lock_do_i_hold(struct lock *lock)
{
	KASSERT(lock != NULL);

	return lock->owner == curthread;
}

////////////////////////////////////////////////////////////
//
// CV


struct cv *
cv_create(const char *name)
{
        struct cv *cv;

        cv = kmalloc(sizeof(struct cv));
        if (cv == NULL) {
                return NULL;
        }

        cv->cv_name = kstrdup(name);
        if (cv->cv_name==NULL) {
                kfree(cv);
                return NULL;
        }
        
        // add stuff here as needed
        
	//initialize the wait channel first, because like malloc and kstrdup,
	//if it fails we bail out	
	cv->wc = wchan_create(cv->cv_name);

	if (cv->wc == NULL) 
	{
		kfree(cv->cv_name);
		kfree(cv);
		return NULL;
	}

        return cv;
}

void
cv_destroy(struct cv *cv)
{
        KASSERT(cv != NULL);

        // add stuff here as needed
        
	wchan_destroy(cv->wc);
        kfree(cv->cv_name);
        kfree(cv);
}

void
cv_wait(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);
	KASSERT(lock_do_i_hold(lock)); //we have to own the lock before we wait. since 
	//we are going to release it.
	

	//yes, technically, lock_release() asserts the same condition. but there are many arguments
	//for why asserting twice, once in this cv function and the other in the lock implementation,
	//is better than being minimalistic. for one, debugging and readability is improved
	

	//we must go to sleep and release the lock, atomically as possible 
	
	//lock the wait channel. we don't want other threads picking up the lock
	//before we release it.
	wchan_lock(cv->wc);

	//release the lock before we go to sleep. this is so other threads can wake us up,
	//when there is a change of state, and the condition we are waiting on that
	//was false before might now be true
	lock_release(lock);

	wchan_sleep(cv->wc);

	lock_acquire(lock);
}

void
cv_signal(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	//we don't need to own the lock when calling signal. why?
	wchan_wakeone(cv->wc);

	(void) lock; //dont need the lock. just call wakeone..
}

void
cv_broadcast(struct cv *cv, struct lock *lock)
{
	KASSERT(cv != NULL);
	KASSERT(lock != NULL);

	wchan_wakeall(cv->wc);

	(void) lock;
}
