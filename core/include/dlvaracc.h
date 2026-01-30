/*
 *  Copyright (C) SplinterGU (Fenix/BennuGD) (Since 2006)
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

/* --------------------------------------------------------------------------- */

#ifndef _DLVARACC_H
#define _DLVARACC_H

/* --------------------------------------------------------------------------- */

#include <stdint.h>

/* --------------------------------------------------------------------------- */

#undef LOCQWORD
#undef LOCDWORD
#undef LOCWORD
#undef LOCBYTE

#undef LOCINT64
#undef LOCINT32
#undef LOCINT16
#undef LOCINT8

#undef LOCUINT64
#undef LOCUINT32
#undef LOCUINT16
#undef LOCUINT8

#undef LOCFLOAT
#undef LOCDOUBLE

#undef GLOQWORD
#undef GLODWORD
#undef GLOWORD
#undef GLOBYTE

#undef GLOINT64
#undef GLOINT32
#undef GLOINT16
#undef GLOINT8

#undef GLOUINT64
#undef GLOUINT32
#undef GLOUINT16
#undef GLOUINT8

#undef GLOFLOAT
#undef GLODOUBLE

/* --------------------------------------------------------------------------- */
/* Macros for accessing local data of an instance                              */
/* --------------------------------------------------------------------------- */

#define LOCEXISTS(m,a)    (m##_locals_fixup[a].size != -1)
#define GLOEXISTS(m,a)    (m##_globals_fixup[a].data_offset)

#define LOCADDR(m,a,b)    ((uint8_t *)(intptr_t)((a)->locdata) + (uint64_t)(intptr_t)m##_locals_fixup[b].data_offset)
#define GLOADDR(m,a)      (m##_globals_fixup[a].data_offset)

#ifdef VITA
/* Vita (ARM) requires aligned access or special handling for unaligned data */
typedef uint64_t u64_unaligned __attribute__((aligned(1)));
typedef int64_t  s64_unaligned __attribute__((aligned(1)));
typedef double   dbl_unaligned __attribute__((aligned(1)));
typedef uint32_t u32_unaligned __attribute__((aligned(1)));
typedef int32_t  s32_unaligned __attribute__((aligned(1)));
typedef float    flt_unaligned __attribute__((aligned(1)));
typedef uint16_t u16_unaligned __attribute__((aligned(1)));
typedef int16_t  s16_unaligned __attribute__((aligned(1)));

#define LOCQWORD(m,a,b)   (*(u64_unaligned *)LOCADDR(m,a,b))
#define LOCDWORD(m,a,b)   (*(u32_unaligned *)LOCADDR(m,a,b))
#define LOCWORD(m,a,b)    (*(u16_unaligned *)LOCADDR(m,a,b))
#define LOCBYTE(m,a,b)    (*(uint8_t  *)LOCADDR(m,a,b))

#define LOCINT64(m,a,b)   (*(s64_unaligned  *)LOCADDR(m,a,b))
#define LOCINT32(m,a,b)   (*(s32_unaligned  *)LOCADDR(m,a,b))
#define LOCINT16(m,a,b)   (*(s16_unaligned  *)LOCADDR(m,a,b))
#define LOCINT8(m,a,b)    (*(int8_t   *)LOCADDR(m,a,b))

#define LOCUINT64(m,a,b)  LOCQWORD(m,a,b)
#define LOCUINT32(m,a,b)  LOCDWORD(m,a,b)
#define LOCUINT16(m,a,b)  LOCWORD(m,a,b)
#define LOCUINT8(m,a,b)   LOCBYTE(m,a,b)

#define LOCFLOAT(m,a,b)   (*(flt_unaligned    *)LOCADDR(m,a,b))
#define LOCDOUBLE(m,a,b)  (*(dbl_unaligned   *)LOCADDR(m,a,b))


#define GLOQWORD(m,a)     (*(u64_unaligned *)GLOADDR(m,a))
#define GLODWORD(m,a)     (*(u32_unaligned *)GLOADDR(m,a))
#define GLOWORD(m,a)      (*(u16_unaligned *)GLOADDR(m,a))
#define GLOBYTE(m,a)      (*(uint8_t  *)GLOADDR(m,a))

#define GLOINT64(m,a)     (*(s64_unaligned  *)GLOADDR(m,a))
#define GLOINT32(m,a)     (*(s32_unaligned  *)GLOADDR(m,a))
#define GLOINT16(m,a)     (*(s16_unaligned  *)GLOADDR(m,a))
#define GLOINT8(m,a)      (*(int8_t   *)GLOADDR(m,a))

#define GLOUINT64(m,a)    GLOQWORD(m,a)
#define GLOUINT32(m,a)    GLODWORD(m,a)
#define GLOUINT16(m,a)    GLOWORD(m,a)
#define GLOUINT8(m,a)     GLOBYTE(m,a)

#define GLOFLOAT(m,a)     (*(flt_unaligned    *)GLOADDR(m,a))
#define GLODOUBLE(m,a)    (*(dbl_unaligned   *)GLOADDR(m,a))

#else

#define LOCQWORD(m,a,b)   (*(uint64_t *)LOCADDR(m,a,b))
#define LOCDWORD(m,a,b)   (*(uint32_t *)LOCADDR(m,a,b))
#define LOCWORD(m,a,b)    (*(uint16_t *)LOCADDR(m,a,b))
#define LOCBYTE(m,a,b)    (*(uint8_t  *)LOCADDR(m,a,b))

#define LOCINT64(m,a,b)   (*(int64_t  *)LOCADDR(m,a,b))
#define LOCINT32(m,a,b)   (*(int32_t  *)LOCADDR(m,a,b))
#define LOCINT16(m,a,b)   (*(int16_t  *)LOCADDR(m,a,b))
#define LOCINT8(m,a,b)    (*(int8_t   *)LOCADDR(m,a,b))

#define LOCUINT64(m,a,b)  LOCQWORD(m,a,b)
#define LOCUINT32(m,a,b)  LOCDWORD(m,a,b)
#define LOCUINT16(m,a,b)  LOCWORD(m,a,b)
#define LOCUINT8(m,a,b)   LOCBYTE(m,a,b)

#define LOCFLOAT(m,a,b)   (*(float    *)LOCADDR(m,a,b))
#define LOCDOUBLE(m,a,b)  (*(double   *)LOCADDR(m,a,b))


#define GLOQWORD(m,a)     (*(uint64_t *)GLOADDR(m,a))
#define GLODWORD(m,a)     (*(uint32_t *)GLOADDR(m,a))
#define GLOWORD(m,a)      (*(uint16_t *)GLOADDR(m,a))
#define GLOBYTE(m,a)      (*(uint8_t  *)GLOADDR(m,a))

#define GLOINT64(m,a)     (*(int64_t  *)GLOADDR(m,a))
#define GLOINT32(m,a)     (*(int32_t  *)GLOADDR(m,a))
#define GLOINT16(m,a)     (*(int16_t  *)GLOADDR(m,a))
#define GLOINT8(m,a)      (*(int8_t   *)GLOADDR(m,a))

#define GLOUINT64(m,a)    GLOQWORD(m,a)
#define GLOUINT32(m,a)    GLODWORD(m,a)
#define GLOUINT16(m,a)    GLOWORD(m,a)
#define GLOUINT8(m,a)     GLOBYTE(m,a)

#define GLOFLOAT(m,a)     (*(float    *)GLOADDR(m,a))
#define GLODOUBLE(m,a)    (*(double   *)GLOADDR(m,a))
#endif


/* --------------------------------------------------------------------------- */

#endif
