/* $Revision: 13632 $ $Date: 2013-04-03 08:46:57 -0700 (Wed, 03 Apr 2013) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 *
 * This file allows the end user to add atomic synchronization operations for
 * an unsupported platform.
 * Do not use this file unless you know you need to.
 *
 * Modify this file to implement these names:
 *   cov_atomic_t
 *   cov_atomic_initializer
 *   cov_atomic_tryLock
 *   cov_atomic_unlock
 */

/* This error occurs because either your platform is not yet supported or a
 * problem occurred recognizing your platform. If your instrumented code is
 * single-threaded with no interrupt handlers, you can use the implementation
 * below. Otherwise, contact Technical Support
 */
#error "No atomic locking implementation available, see comment"

/* Parameter type for cov_atomic_tryLock, below */
typedef volatile int cov_atomic_t;

/* Initialize cov_atomic_t to unlocked state */
#define cov_atomic_initializer 1

/* Attempt to obtain exclusive access, based on this cov_atomic_t.  Return
 *   value is true if caller obtains the lock.
 *
 * Set the cov_atomic_t to locked state, return non-zero if previous state
 *   was unlocked.  See atomic.h for examples in C.  You may implement the
 *   function in assembly language, see atomic-*.s for examples.
 */
static int cov_atomic_tryLock(cov_atomic_t* p)
{
	int result = *p;
	*p = 0;
	return result;
}

/* Set state of the cov_atomic_t to unlocked */
static void cov_atomic_unlock(cov_atomic_t* p)
{
	*p = 1;
}
