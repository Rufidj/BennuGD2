/*
 *  m_ime.h - system on-screen keyboard (PS5 IME dialog)
 *
 *  SYSTEM_KEYBOARD_OPEN( title, initial_text, max_length, flags ) -> 0 if it opened, < 0 if not available / failed
 *        flags: 1 = mask the text (password), 2 = keyboard laid out for web addresses
 *  SYSTEM_KEYBOARD_STATUS()  -> 0 not open, 1 on screen, 2 closed (read the result)
 *  SYSTEM_KEYBOARD_OK()      -> 1 if the user accepted the text (valid once the status is 2)
 *  SYSTEM_KEYBOARD_TEXT()    -> the text (UTF-8)
 *  SYSTEM_KEYBOARD_CLOSE()   -> shuts the dialog down (call after reading the result, or to cancel)
 *
 *  Only the PS5 build has a keyboard; everywhere else OPEN returns < 0 and a script keeps using its own.
 *  The dialog is non-blocking: the script must keep running its frames (and presenting) while it is open.
 */
#ifndef __M_IME_H
#define __M_IME_H

#include "bgddl.h"

extern int64_t libmod_misc_syskb_open( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_syskb_status( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_syskb_ok( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_syskb_text( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_syskb_close( INSTANCE * my, int64_t * params );

/* shared with m_progress.c: brings up libSceCommonDialog + the user service once; a line in /data/ime.log per step */
extern int ps5_dialogs_init( int sysmodule );
extern void ps5_dialog_log( const char * fmt, ... );

#endif
