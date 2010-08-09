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

#include <errno.h>
#include <string.h>

#include "sxe-log.h"
#include "sxe-mmap.h"

unsigned sxe_spinlock_count_max = 1000000; /* number of yields before spinlock fails */

#ifndef _WIN32

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

void
sxe_mmap_open(SXE_MMAP * memmap, const char * file) {
    struct stat st;

    SXEE82("sxe_mmap_open(memmap=%p, file=%s)", memmap, file);
    SXEA11(stat(file, &st) >= 0, "Cannot stat size of file %s", file);
    memmap->size = st.st_size;
    SXEA12((memmap->fd = open(file, O_RDWR, 0666)) >= 0, "Failed to open file %s: %s", file, strerror(errno));
    SXEA12((memmap->addr = mmap(NULL, memmap->size, PROT_READ | PROT_WRITE, MAP_SHARED, memmap->fd, 0)) != MAP_FAILED,
       "Failed to mmap file %s: %s", file, strerror(errno));
    SXER80( "return // sxe_mmap_open()" );
}

void
sxe_mmap_close(SXE_MMAP* memmap) {
    SXEE81("sxe_mmap_close(memmap=%p)", memmap);
    SXEA11(munmap(memmap->addr, memmap->size) >= 0, "Fail to munmap file: %s", strerror(errno));
    close(memmap->fd);
    SXER80("return // sxe_mmap_close()");
}

#else /* _WIN32 is defined */

#include <sys/types.h>
#include <sys/stat.h>
#include <winsock2.h>
#include <windows.h>

void
sxe_mmap_open(SXE_MMAP * memmap, const char * file)
{
    DWORD       access_mode = GENERIC_READ  | GENERIC_WRITE;
    DWORD       map_mode    = PAGE_READWRITE;
    DWORD       view_mode   = FILE_MAP_READ | FILE_MAP_WRITE;
    HANDLE      fh;
    HANDLE      view;
    struct stat st;

    SXEE82("sxe_mmap_open(memmap=%p, file=%s)", memmap, file);
    SXEA11(0 == stat (file, &st), "failed to stat file: %s", file);
    memmap->size = st.st_size;

    fh = CreateFile ( file, access_mode, FILE_SHARE_READ|FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL|FILE_FLAG_RANDOM_ACCESS, NULL );
    SXEA11( fh != INVALID_HANDLE_VALUE, "fail to open file %s", file );

    view = CreateFileMapping ( fh, NULL , map_mode, 0 /* len >> 32 */, 0 /* len & 0xFFFFFFFF */, /* low-order DWORD of size */ 0 );
    SXEA11( view != NULL, "fail to create file mapping for file %s", file );

    memmap->addr = 0;
    memmap->addr = MapViewOfFile ( view, view_mode, 0, 0, memmap->size );
    SXEA11( 0 != memmap->addr, "fail to map view of file for file %s", file );

    memmap->win32_fh   = fh  ;
    memmap->win32_view = view;
    SXEL81("file mapped to address: %p", memmap->addr );
    SXER80("return // sxe_mmap_open()" );
}

void
sxe_mmap_close(SXE_MMAP * memmap)
{
    SXEE81("sxe_mmap_close(memmap=%p)", memmap);

    SXEA10(memmap       != NULL, "invalid parameter");
    SXEA10(memmap->addr != NULL, "invalid struct member");

    SXEA10(UnmapViewOfFile(memmap->addr      ), "fail to unmap view of file");
    SXEA10(CloseHandle    (memmap->win32_view), "fail to close view handle" );
    SXEA10(CloseHandle    (memmap->win32_fh  ), "fail to close file handle" );

    SXER80("return // sxe_mmap_close()" );
}

#endif /* _WIN32 */
