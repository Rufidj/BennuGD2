/*
 *  libmod_video_hwdec - the PS5's video decoder block (libSceVideodec2) for libmod_video.
 *  H.264 and HEVC (8 and 10 bit, up to 4K). Only built for PS5 (__PROSPERO__); elsewhere every call
 *  reports "not available" so the module keeps decoding in software.
 *
 *  Written from the console API's own facts (function names, structure layouts, call order); the
 *  structure of the ps5vdec.c plugin in github.com/ZiZc3/VLC (GPL-2.0+) was used as the reference for
 *  those facts, no code was copied.
 */
#ifndef __LIBMOD_VIDEO_HWDEC_H
#define __LIBMOD_VIDEO_HWDEC_H

#include <stddef.h>
#include <stdint.h>

typedef struct HWDEC HWDEC;

typedef struct {
    const uint8_t * y;          /* luma rows, 'pitch' bytes apart                          */
    const uint8_t * uv;         /* interleaved chroma (NV12 / 16-bit P010-like), same pitch */
    int             pitch;      /* bytes                                                   */
    int             w, h;       /* the decoder's (padded) size                             */
    int             ten;        /* 1: 16-bit samples, value in the low 10 bits             */
    int64_t         pts;        /* what was given with the access unit                     */
} HWPIC;

typedef void ( * hwdec_cb )( void * ud, const HWPIC * pic );

int      hwdec_available( void );
HWDEC *  hwdec_open( int hevc, int w, int h );
void     hwdec_close( HWDEC * d );
/* 0 = ok, <0 = decoder error (stream errors need hwdec_reset + a keyframe) */
int      hwdec_decode( HWDEC * d, const uint8_t * au, size_t size, int64_t pts, hwdec_cb cb, void * ud );
void     hwdec_flush( HWDEC * d, hwdec_cb cb, void * ud );   /* end of stream: pictures still inside */
void     hwdec_reset( HWDEC * d );                           /* seek: drop everything                */

#endif
