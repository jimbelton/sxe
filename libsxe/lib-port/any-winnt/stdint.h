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

/* Simulate ANSI C99 for Visual C++ (another fine MS product) */

#ifndef __SXE_STDINT_H
#define __SXE_STDINT_H

#ifdef MAKE_MINGW
#   include <_mingw.h>
#   include <io.h> // for intptr_t (but not uintptr_t)
#   ifdef _WIN64
    typedef unsigned __int64 uintptr_t;
#   else
    typedef unsigned __int32 uintptr_t;
#   endif
#else
#   include <stddef.h>    /* For [u]intptr_t */
#endif

typedef          __int8  int8_t       ;
typedef          __int16 int16_t      ;
typedef            int   int32_t      ; /* no __int32 because MS defines it as a long whereas linux uses int */
typedef          __int64 int64_t      ;
typedef            int   int_least16_t;
typedef unsigned __int8  uint8_t      ;
typedef unsigned __int16 uint16_t     ;
typedef unsigned         uint32_t     ; /* no __int32 because MS defines it as a long whereas linux uses int */
typedef unsigned __int64 uint64_t     ;

#endif
