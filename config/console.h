#ifndef CONFIG_CONSOLE_H
#define CONFIG_CONSOLE_H

/** @file
 *
 * Console configuration
 *
 * These options specify the console types that Etherboot will use for
 * interaction with the user.
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

#include <config/defaults.h>

#ifdef MLX_DEBUG
#define	CONSOLE_PCBIOS		/* Default BIOS console */
#define	CONSOLE_SERIAL		/* Serial port */
#endif
//#define	CONSOLE_DIRECT_VGA	/* Direct access to VGA card */
//#define	CONSOLE_PC_KBD		/* Direct access to PC keyboard */
//#define	CONSOLE_SYSLOG		/* Syslog console */
//#define	CONSOLE_SYSLOGS		/* Encrypted syslog console */
//#define	CONSOLE_VMWARE		/* VMware logfile console */
//#define	CONSOLE_DEBUGCON	/* Debug port console */
//#define	CONSOLE_VESAFB		/* VESA framebuffer console */

#define	KEYBOARD_MAP	us

#ifdef MLX_DEBUG
#define	LOG_LEVEL	LOG_ALL
#else
#define	LOG_LEVEL	LOG_NONE
#endif

#include <config/named.h>
#include NAMED_CONFIG(console.h)
#include <config/local/console.h>
#include LOCAL_NAMED_CONFIG(console.h)

#endif /* CONFIG_CONSOLE_H */
