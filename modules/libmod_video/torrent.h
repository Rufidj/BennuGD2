/*
 *  torrent.h - BitTorrent client engine for streaming (part of libmod_video)
 *
 *  A torrent (a .torrent file, an http(s) address of one, or a magnet: link) is downloaded in the background:
 *  the pieces of the file being watched come first, in order, from the place being read. The files are served
 *  on a local HTTP address (127.0.0.1) with Range support, so the video player opens them like any other
 *  stream (the request waits until the pieces it needs have arrived).
 *
 *  Everything runs on its own threads; the functions below only take a lock for a moment and never block,
 *  except torrent_stop (waits for the connections to close).
 */
#ifndef __TORRENT_H
#define __TORRENT_H

#include <stdint.h>

/* cache_dir: where the downloaded data is kept (created if missing). Returns 0 or a negative error. */
int      torrent_init( const char * cache_dir );
void     torrent_shutdown( void );

/* Starts a torrent. src: "magnet:?xt=...", a path to a .torrent, or an http(s) address of one.
 * Returns a handle (>= 1), or a negative number (-1 no free slot, -2 bad source). */
int      torrent_add( const char * src );
void     torrent_remove( int h, int delete_data );

/* 0 = looking for the file list (magnet) / reading the .torrent; 1 = ready; 2 = failed (see torrent_error) */
int      torrent_state( int h );
const char * torrent_error( int h );
const char * torrent_name( int h );

int      torrent_file_count( int h );
const char * torrent_file_name( int h, int i );
int64_t  torrent_file_size( int h, int i );
/* index of the biggest video-looking file (what you would play), or -1 */
int      torrent_main_file( int h );
/* the file to download first (the rest is not downloaded) */
int      torrent_select( int h, int i );
/* local address that serves file i ("http://127.0.0.1:PORT/H/I/name"); written into out */
int      torrent_url( int h, int i, char * out, int outlen );

/* numbers for a progress display */
int      torrent_peers( int h );                 /* connected peers */
int      torrent_down_rate( int h );             /* bytes per second */
int      torrent_up_rate( int h );
int      torrent_file_percent( int h );          /* 0..100 of the selected file that is on disk */
int64_t  torrent_buffered( int h );              /* bytes ready right after the position being played */
int      torrent_start_percent( int h );         /* how much of what a player needs to open the file is ready, 0..100 */
int64_t  torrent_downloaded( int h );            /* total bytes received */

/* bytes per second the client may send to others (0 = never upload; 0 is the default) */
void     torrent_set_upload_limit( int bytes_per_sec );

/* extra_trackers: add a few public trackers to every torrent (on by default). dht: also look for peers in the DHT, without
 * trackers (on by default; read-only: it only asks, it never answers other nodes). Set before torrent_add(). */
void     torrent_set_options( int extra_trackers, int dht );
/* how many MB past the play position are downloaded (64..8192, 192 by default): raise it when a big buffer is needed */
void     torrent_set_ahead( int mb );

#endif
