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

/* This package is loosely based on code from http://homepages.inf.ed.ac.uk/s0450736/maxent.html
 * The license in the source file is MIT
 * See: http://www.google.com/codesearch/p?hl=en#YZclbNWHoEk/s0450736/software/maxent/maxent-20061005.tar.bz2|Xqc-gC9Batk/maxent-20061005/src/mmapfile.h
 */

#ifndef __SXE_MMAP__
#define __SXE_MMAP__

#ifdef _WIN32
    #include <winsock2.h>
    #include <windows.h>
#endif

typedef struct {
    unsigned long size;
    int           fd;
    void        * addr;
    int           flags;
#ifdef _WIN32
    HANDLE        win32_fh;
    HANDLE        win32_view;
#endif
} SXE_MMAP;

#define SXE_MMAP_ADDR(memmap) ((volatile void *)(memmap)->addr)

#include "sxe-mmap-proto.h"

#endif
