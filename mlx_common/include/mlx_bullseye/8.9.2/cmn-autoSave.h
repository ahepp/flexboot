/* $Revision: 13778 $ $Date: 2013-06-18 16:28:02 -0700 (Tue, 18 Jun 2013) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 */

#if __cplusplus
	extern "C" {
#endif

/* Set this to 0 to prevent the auto-save thread creation. */
int cov_autoSave = 1;

#if __cplusplus
	}
#endif

#if Libcov_noAutoSave || Libcov_dynamic
	static int autoSave_create(void)
	{
		return 0;
	}

	static void autoSave_join(void)
	{
	}
#else
	static pthread_t autoSave_pthread;
	static volatile char autoSave_isRun;
	static int autoSave_pthread_isValid;

	#if !defined(PTHREAD_LINK)
		#define PTHREAD_LINK
	#endif
	static void* PTHREAD_LINK autoSave_thread(void* arg)
	{
		unsigned count = 0;
		#if !_WIN32 && (_AIX || __APPLE__ || __FreeBSD__ || __hpux || __linux || __osf__ || __sgi || __sun || \
				defined(_POSIX_C_SOURCE) || defined(_XOPEN_SOURCE) || defined(SIG_SETMASK))
			/* Block all signals */
			sigset_t signalMask;
			sigfillset(&signalMask);
			pthread_sigmask(SIG_SETMASK, &signalMask, NULL);
		#endif
		while (autoSave_isRun) {
			/* Sleep for 1/10th second, practically imperceptible to a human */
			struct timespec rqt;
			int status;
			rqt.tv_sec = 0;
			rqt.tv_nsec = 100000000;
			/*   Repeat if interrupted by a signal */
			do {
				status = nanosleep(&rqt, NULL);
			} while (status != 0 && errno == EINTR);
			/*   If nanosleep is not working */
			if (status != 0) {
				/* Bail out rather than consume the CPU */
				break;
			}
			/* Call cov_write every 10 loops, making once per second */
			count++;
			if (count == 10 && autoSave_isRun) {
				cov_write();
				count = 0;
			}
		}
		autoSave_pthread_isValid = 0;
		pthread_exit(NULL);
		arg = arg;
		return NULL;
	}

	static int autoSave_create(void)
	{
		int status = 0;
		const char* p = getenv("COVAUTOSAVE");
		if ((p == NULL || p[0] != '0' || p[1] != '\0') && cov_autoSave) {
			autoSave_isRun = 1;
			status = pthread_create(&autoSave_pthread, NULL, autoSave_thread, NULL);
			if (status == 0) {
				autoSave_pthread_isValid = 1;
			} else if (status == ENOSYS) {
				status = 0;
			}
		}
		return status;
	}

	static void autoSave_join(void)
	{
		autoSave_isRun = 0;
		#if _AIX
			/* On AIX 5.1, pthread_join and nanosleep incorrectly let a blocked signal be delivered
			// Do nothing. The thread will be cleaned up when the application terminates
			*/
		#elif Libcov_posix
			/* of pthread_join never returns on the systems listed below. Use nanosleep instead
			// - uclibc
			// - DENX Embedded Linux Development Kit 4.2 ppc_4xx
			// Wait maximum 5 seconds, in case the thread is not running at all.
			// If we give up too soon, the only risk is valgrind --leak-check=full reports a memory leak by pthread_create.
			*/
			{
				unsigned count;
				struct timespec rqt;
				rqt.tv_sec = 0;
				rqt.tv_nsec = 10000000;
				for (count = 0; autoSave_pthread_isValid && count < 500; count++) {
					nanosleep(&rqt, NULL);
				}
			}
		#else
			if (autoSave_pthread_isValid) {
				pthread_join(autoSave_pthread, NULL);
			}
		#endif
	}

#if Libcov_posix
	static void autoSave_atfork(void)
	{
		autoSave_pthread_isValid = 0;
	}
#endif
#endif
