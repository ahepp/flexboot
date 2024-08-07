/* $Revision: 14428 $ $Date: 2014-01-30 16:51:42 -0800 (Thu, 30 Jan 2014) $
// Copyright (c) Bullseye Testing Technology
// This source file contains confidential proprietary information.
//
// BullseyeCoverage small footprint run-time porting layer
// http://www.bullseye.com/help/env-embedded.html
//
// Implementations should conform to the Single UNIX Specification
// http://www.opengroup.org/onlinepubs/009695399/
*/

#ifdef MLX_BULLSEYE

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <byteswap.h>
#include <ipxe/io.h>
#include <ipxe/pci.h>
#include <ipxe/pcibackup.h>
#include <ipxe/malloc.h>
#include <ipxe/umalloc.h>
#include <ipxe/iobuf.h>
#include <ipxe/in.h>
#include <ipxe/netdevice.h>
#include <ipxe/process.h>
#include <ipxe/infiniband.h>
#include <ipxe/ib_smc.h>
#include <ipxe/if_ether.h>
#include <ipxe/ethernet.h>
#include <ipxe/settings.h>
#include <ipxe/fcoe.h>
#include <ipxe/vlan.h>
#include <ipxe/bofm.h>
#include <ipxe/iscsi.h>
#include <ipxe/settings.h>
#include <ipxe/status_updater.h>
#include <config/general.h>

#if _BullseyeCoverage
	//#warning BullseyeCoverage is off!
	#pragma BullseyeCoverage off
#endif

/*---------------------------------------------------------------------------
// NULL
*/
#if 1
	#include <stdlib.h>
	#include <stdio.h>
	#include <string.h>
#endif

/*---------------------------------------------------------------------------
// open, close, write
*/
#if 0
	#include <fcntl.h>
	#include <sys/stat.h>
	#include <unistd.h>
	#if !defined(O_CREAT)
		#error O_CREAT not defined
	#endif
	#if !defined(S_IRUSR)
		#error S_IRUSR not defined
	#endif
#else
	/* http://pubs.opengroup.org/onlinepubs/009695399/functions/open.html
	// http://pubs.opengroup.org/onlinepubs/009695399/functions/close.html
	// http://pubs.opengroup.org/onlinepubs/009695399/functions/write.html
	*/
	#define O_CREAT 0x0100
	#define O_TRUNC 0x0200
	#define O_WRONLY 1
	#define S_IRUSR 0x0100
	#define S_IWUSR 0x0080

	int getpid(void)
	{
		/* If you have multiple instrumented programs running simultaneously,
		// this function should return a different value for each of them.
		// Otherwise, return any value.
		*/
		return 1;
	}

#define MLX_BULLSEYE_STRING_(x) #x
#define MLX_BULLSEYE_STRING(x) MLX_BULLSEYE_STRING_(x)

static unsigned int bullseye_position = 0;
#ifdef MLX_QEMU
#define bullseye_printf printf
#else
static void bullseye_printf ( const char *fmt, ... )
{
	va_list args;
	char buffer[1024];

	unsigned int i;

	va_start ( args, fmt );
	vsnprintf(buffer, sizeof(buffer), fmt, args);
	va_end ( args );

	/*
	 *  Print each character by itself
	 *  twice in order to overcome breaked messages
	 */

	for( i=0; i<strlen(buffer); i++ ) {
		printf("[%08x:%02x]\n", bullseye_position, (unsigned char)buffer[i]);
		printf("[%08x:%02x]\n", bullseye_position, (unsigned char)buffer[i]);
		bullseye_position++;
	}
}
#endif

	int mlx_bullseye_open(const char* path __attribute__((unused)) , int oflag __attribute__((unused)) , int mode __attribute__((unused)) )
	{
		/* Insert your code here. Successful return is > 2 */
		bullseye_position = 0;
		bullseye_printf("%s - BULLSEYE_START\n",MLX_BULLSEYE_STRING(__BULLSEYE_ROM_FILE_NAME__));
		return 3;
	}

	int mlx_bullseye_close(int fildes __attribute__((unused)) )
	{
		bullseye_printf("%s - BULLSEYE_END\n",MLX_BULLSEYE_STRING(__BULLSEYE_ROM_FILE_NAME__));
		return 0;
	}

	int mlx_bullseye_write(int fildes __attribute__((unused)), const void* buf, unsigned nbyte)
	{
		bullseye_printf("%s",(char *)buf);
		return nbyte;
	}

#endif

/*---------------------------------------------------------------------------
// BullseyeCoverage run-time code
*/
#include "atomic.h"
#include "libcov.h"
#include "version.h"
#include "libcov-core-small.h"
#endif
