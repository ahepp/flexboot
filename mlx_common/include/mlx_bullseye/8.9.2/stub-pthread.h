/* $Revision: 11867 $ $Date: 2011-03-18 12:11:41 -0700 (Fri, 18 Mar 2011) $
 * Copyright (c) Bullseye Testing Technology
 */

#define pthread_t Libcov_pthread_t
typedef int pthread_t;

#if Libcov_posix && !Libcov_noAutoSave
#define pthread_atfork Libcov_pthread_atfork
static int pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	prepare = prepare;
	parent = parent;
	child = child;
	return ENOSYS;
}
#endif

#define pthread_create Libcov_pthread_create
static int pthread_create(pthread_t* thread, const void* attr, void* (* start_routine)(void*), void* arg)
{
	thread = thread;
	attr = attr;
	start_routine = start_routine;
	arg = arg;
	return ENOSYS;
}

#define pthread_exit Libcov_pthread_exit
static void pthread_exit(void* value_ptr)
{
	value_ptr = value_ptr;
}

#define pthread_join Libcov_pthread_join
static int pthread_join(pthread_t thread, void** value_ptr)
{
	thread = thread;
	value_ptr = value_ptr;
	return ENOSYS;
}
