/*
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)
 *  Copyright (C) 2002-2006 Fenix Team (Fenix)
 *  Copyright (C) 1999-2002 José Luis Cebrián Pagüe (Fenix)
 *
 *  This file is part of Bennu Game Development
 *
 *  This software is provided 'as-is', without any express or implied
 *  warranty. In no event will the authors be held liable for any damages
 *  arising from the use of this software.
 *
 *  Permission is granted to anyone to use this software for any purpose,
 *  including commercial applications, and to alter it and redistribute it
 *  freely, subject to the following restrictions:
 *
 *     1. The origin of this software must not be misrepresented; you must not
 *     claim that you wrote the original software. If you use this software
 *     in a product, an acknowledgment in the product documentation would be
 *     appreciated but is not required.
 *
 *     2. Altered source versions must be plainly marked as such, and must not be
 *     misrepresented as being the original software.
 *
 *     3. This notice may not be removed or altered from any source
 *     distribution.
 *
 */

#ifndef __TOKEN_H
#define __TOKEN_H

#include <files.h>

#define MAX_SOURCES         4096
#define MAX_MACRO_PARAMS    256

/* Tokenizer */

/* Token types */
#define IDENTIFIER 1
#define STRING     2
#define NUMBER     3
#define FLOAT      4
#define LABEL      5
#define NOTOKEN    6

extern struct _token {
        int type;
        int64_t code;
        double value;
        int file;
        int line;
    } token;

typedef struct _tok_pos {
        int                 use_saved;
        struct _token       token;
        struct _token       token_saved;
        struct _token       token_prev;
        int                 line_count;
        int                 current_file;
        const unsigned char *source_ptr;
    } tok_pos;

extern void token_init( const unsigned char * source, int file );
extern void token_next();
extern void token_back();
extern void token_dump();

extern tok_pos token_pos();
extern void token_set_pos( tok_pos tp );

extern void add_simple_define( unsigned char * macro, unsigned char *text );

extern int line_count;
extern int current_file;
extern int n_files;
extern unsigned char files[MAX_SOURCES][__MAX_PATH];

/* All tokens are exported */
extern struct _token token;
extern struct _token token_prev;
extern struct _token token_saved;

#endif
