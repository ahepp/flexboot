/* $Revision: 13316 $ $Date: 2012-12-03 17:36:30 -0800 (Mon, 03 Dec 2012) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 *
 * This implementation of fcntl(F_SETLW,...) creates and checks for a
 * lock file with the open flag O_EXCL to provide exclusive access to a
 * file from the first lock request until the file is closed.
 * Only one file can be locked at a time, which is all we need.
 */

#include <time.h>

#define flock Libcov_fcntlExcl_flock
struct flock {
	short l_type;
	short l_whence;
	off_t l_start;
	off_t l_len;
};
#undef F_SETLKW
#undef F_RDLCK
#undef F_UNLCK
#undef F_WRLCK
#define F_SETLKW 0
#define F_RDLCK 0
#define F_UNLCK 1
#define F_WRLCK 2

/* The file descriptor for the file currently locked, -1 if no file is locked */
static int lockFildes = -1;
/* The path of the lock file */
static char lockPath[PATH_MAX];

/* fcntl */
static int Libcov_fcntlExcl_fcntl(int fildes, int cmd, ...)
{
	/* If first lock request */
	if (lockFildes < 0) {
		lockFildes = fildes;
		/* Wait until the lock file does not exist.
		 * If the lock file existed at program startup, for example because the test
		 * program previously terminated abnormally, manually remove the lock file.
		 * Otherwise this loop never terminates.
		 */
		for (;;) {
			/* If we succeed in creating the lock file */
			const int fd = open(lockPath, O_CREAT | O_EXCL, 0666);
			clock_t end;
			if (fd >= 0) {
				/* Close it and stop */
				close(fd);
				break;
			}
			/* Wait 0.1 second */
			end = clock();
			if (end != (clock_t)-1) {
				end += CLOCKS_PER_SEC / 10;
				while (clock() < end)
					;
			}
		}
	}
	fildes = fildes;
	cmd = cmd;
	return 0;
}
#define fcntl Libcov_fcntlExcl_fcntl

/* open */
static int Libcov_fcntlExcl_open(const char* path, int oflag, mode_t mode)
{
	/* If no file is locked */
	if (lockFildes < 0) {
		strcpy(lockPath, path);
		strcat(lockPath, ".lock");
	}
	return open(path, oflag | O_BINARY, mode);
}
#undef open
#define open Libcov_fcntlExcl_open

/* close */
static int Libcov_fcntlExcl_close(int fildes)
{
	int status1 = close(fildes);
	int status2 = 0;
	/* If this file was locked */
	if (fildes == lockFildes) {
		status1 = remove(lockPath);
		lockFildes = -1;
	}
	return status1 != 0 ? status1 : status2;
}
#define close Libcov_fcntlExcl_close

