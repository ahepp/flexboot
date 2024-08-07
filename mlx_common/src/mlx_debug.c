/*
 * mlx_debug.c
 *
 *  Created on: Dec 16, 2014
 *      Author: moshikos
 */

#pragma GCC push_options
#pragma GCC optimize 0

#ifdef MLX_DEBUG

#include <stdio.h>
#include <ipxe/gdbserial.h>
#include <ipxe/gdbstub.h>
#include "mlx_debug.h"

static int mlx_debug_initialized = 0;

void MlxDebugBreakSerial()
{
	if ( mlx_debug_initialized ) {
		printf ( "\n\n"
				"############################################################\n"
				"Software breakpoint\n"
				"############################################################\n"
				);
		gdbmach_breakpoint();
	} else
	{
		MlxDebugInitializeSerial();
		mlx_debug_initialized = 1;
	}
}

void MlxDebugInitializeSerial()
{
	printf ( "\n\n"
				"############################################################\n"
				"Init debugger: Waiting to sync with debugger on serial port\n"
				"############################################################\n"
				);
	gdbstub_start ( gdbserial_configure() );
	printf ( "System synched with debugger\n"
			 "############################################################\n");
}

void _MlxDebugWaitInfinite(void)
{
	volatile int i = 0;

	printf ( "\n\n"
				"############################################################\n"
				"Init debugger: Waiting to infinite loop while i=0\n"
				"############################################################\n"
				);
	while ( i == 0 )
		;
}

#else

#endif

#pragma GCC pop_options
