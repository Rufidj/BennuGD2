/*
 *  upload.h - receive files from another computer through a web page (part of libmod_video)
 *
 *  A small web server runs on its own threads. Opening its address in a browser shows a page where files are
 *  dropped; each one is sent with a plain POST and written into the chosen folder. A PIN that the person at the
 *  console reads out protects it: without it nothing is written. Nothing is ever overwritten (the new file gets a
 *  free name like "video (2).mp4"), only the folder given to upload_start() is written, and the name of every file
 *  is reduced to a safe base name.
 */
#ifndef __UPLOAD_H
#define __UPLOAD_H

#include <stdint.h>

/* folder: where the files go (must exist). port: first port to try (the next 19 are tried too).
 * pin: text the browser has to give. Returns the port that is listening, or a negative number. */
int          upload_start( const char * folder, int port, const char * pin );
void         upload_stop( void );
int          upload_running( void );

int          upload_file_count( void );          /* files received since the start */
int64_t      upload_total_bytes( void );         /* bytes received since the start */
int          upload_percent( void );             /* of the file being received, 0..100 (-1 = none right now) */
const char * upload_current( void );             /* its name ("" = none) */
const char * upload_last_error( void );
/* address of this computer on the local network ("192.168.1.50"), "" if it cannot be found */
const char * upload_local_ip( void );

#endif
