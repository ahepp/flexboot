#ifndef _USR_PROMPT_H
#define _USR_PROMPT_H

/** @file
 *
 * Prompt for keypress
 *
 */

FILE_LICENCE ( GPL2_OR_LATER );

extern int prompt ( const char *text, unsigned long timeout, int key );
extern int prompt_any ( const char *text, unsigned long timeout );

#endif /* _USR_PROMPT_H */
