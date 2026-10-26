/*
 *  m_progress.h - system progress bar (PS5 message dialog in progress mode)
 *
 *  SYSTEM_PROGRESS_OPEN( caption, flags ) -> 0 if it opened, < 0 if not available / failed (flags: 1 = with a cancel button)
 *  SYSTEM_PROGRESS_SET( percent )  -> moves the bar (0..100)
 *  SYSTEM_PROGRESS_MESSAGE( text ) -> changes the caption
 *  SYSTEM_PROGRESS_UPDATE()        -> advances the dialog; call once per frame while it is open (0 = not open, 3 = it was closed: the user
 *                                     pressed cancel)
 *  SYSTEM_PROGRESS_CLOSE()         -> closes it
 *
 *  Only the PS5 build has one; elsewhere OPEN returns < 0 (or, with MP_FAKE_PROGRESS set, writes what it would do to stderr).
 */
#ifndef __M_PROGRESS_H
#define __M_PROGRESS_H

#include "bgddl.h"

extern int64_t libmod_misc_progress_open( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_progress_set( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_progress_message( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_progress_update( INSTANCE * my, int64_t * params );
extern int64_t libmod_misc_progress_close( INSTANCE * my, int64_t * params );

#endif
