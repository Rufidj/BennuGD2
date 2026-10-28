/*
 *  dlna.h - browse the media servers of the local network (UPnP / DLNA "ContentDirectory"), part of libmod_video
 *
 *  dlna_scan() looks for servers in the background (SSDP); dlna_browse() asks one of them for the content of a folder, also in
 *  the background. The files of a server are played through the plain http address it gives, so the player opens them like any stream.
 *  Nothing here is ever written: it only reads what the servers offer.
 */
#ifndef __DLNA_H
#define __DLNA_H

/* start looking for servers (about 3 s). 0 = started, -1 = already running */
int          dlna_scan( void );
int          dlna_scan_state( void );        /* 0 never started, 1 running, 2 done */
int          dlna_count( void );
const char * dlna_name( int i );

/* ask server i for the children of object `id` ("0" = the root), `count` of them from `start`. 0 = started, -1 = busy / bad server */
int          dlna_browse( int i, const char * id, int start, int count );
int          dlna_browse_state( void );      /* 0 never started, 1 running, 2 done, 3 failed */
const char * dlna_result( void );            /* lines: "C<TAB>id<TAB>title<TAB>children" folders, "I<TAB>url<TAB>title<TAB>bytes<TAB>seconds<TAB>kind" files (kind v, a or i) */
int          dlna_total( void );             /* how many children the folder has in total */
const char * dlna_error( void );

#endif
