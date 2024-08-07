/* $Revision: 13714 $ $Date: 2013-05-16 14:20:53 -0700 (Thu, 16 May 2013) $
 * Copyright (c) Bullseye Testing Technology
 * This source file contains confidential proprietary information.
 *
 * Implementations of open, close, read, write, lseek using functionality in <stdio.h>
 */

#include <stdio.h>
#define O_CREAT 0x0100
#define O_RDONLY 0
#define O_RDWR 2
#define O_TRUNC 0x0200
#define O_WRONLY 1
#define S_IRUSR 0x0100
#define S_IWUSR 0x0080

int open(const char* path, int oflag, int mode)
{
	int fd = -1;
	if (sizeof(int) >= sizeof(FILE*)) {
		FILE* file;
		const char* m;
		switch (oflag & (O_CREAT | O_RDONLY | O_RDWR | O_WRONLY)) {
		case O_RDONLY:
		default:
			m = "r";
			break;
		case O_RDWR:
			m = "r+b";
			break;
		case O_CREAT | O_WRONLY:
			m = "w";
			break;
		case O_CREAT | O_RDWR:
			m = "w+b";
			break;
		}
		file = fopen(path, m);
		if (file != NULL) {
			fd = (int)file;
		}
	}
	return fd;
}

int close(int fildes)
{
	return fclose((FILE*)fildes);
}

int read(int fildes, void* buf, unsigned nbyte)
{
	int n = (int)fread(buf, 1, nbyte, (FILE*)fildes);
	if (n == 0 && nbyte != 0) {
		n = -1;
	}
	return n;
}

int write(int fildes, const void* buf, unsigned nbyte)
{
	int n;
	if (fildes == 2) {
		fildes = (int)stderr;
	}
	n = (int)fwrite(buf, 1, nbyte, (FILE*)fildes);
	if (n == 0 && nbyte != 0) {
		n = -1;
	}
	return n;
}

off_t lseek(int fildes, off_t offset, int whence)
{
	off_t off = -1;
	if (fseek((FILE*)fildes, offset, whence) == 0) {
		off = ftell((FILE*)fildes);
	}
	return off;
}
