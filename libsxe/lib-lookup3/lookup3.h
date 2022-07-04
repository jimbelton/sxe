/* Copyright (c) 2010 Sophos Group.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __LOOKUP3_H__
#define __LOOKUP3_H__

#ifdef _WIN32    /* Sophos extension: define endianness for X86 and X64 versions of Windows */
    #if defined(_M_IX86) || defined(_M_X64)
        #define __LITTLE_ENDIAN 1
        #define __BYTE_ORDER    __LITTLE_ENDIAN
    #endif
#else
    #include <sys/param.h>  /* attempt to define endianness */
    #ifdef linux
        #include <endian.h>    /* attempt to define endianness */
    #endif
#endif

#if defined(__APPLE__)
#    define __LITTLE_ENDIAN LITTLE_ENDIAN
#    define __BYTE_ORDER    BYTE_ORDER
#endif

/*
 * My best guess at if you are big-endian or little-endian.  This may need adjustment.
 */
#if (defined(__BYTE_ORDER__) && defined(__LITTLE_ENDIAN__) && __BYTE_ORDER__ == __LITTLE_ENDIAN__) || \
    (defined(__BYTE_ORDER) && defined(__LITTLE_ENDIAN) && __BYTE_ORDER == __LITTLE_ENDIAN) || \
    (defined(i386) || defined(__i386__) || defined(__i486__) || \
     defined(__i586__) || defined(__i686__) || defined(vax) || defined(MIPSEL))
# define HASH_LITTLE_ENDIAN 1
# define HASH_BIG_ENDIAN 0
#elif (defined(__BYTE_ORDER__) && defined(__BIG_ENDIAN__) && __BYTE_ORDER__ == __BIG_ENDIAN__) || \
      (defined(__BYTE_ORDER) && defined(__BIG_ENDIAN) && __BYTE_ORDER == __BIG_ENDIAN) || \
      (defined(sparc) || defined(POWERPC) || defined(mc68000) || defined(sel))
# define HASH_LITTLE_ENDIAN 0
# define HASH_BIG_ENDIAN 1
#else
# error "Can't determine the endianess"
#endif

#include <stdint.h>
#include "lookup3-proto.h"
#endif
