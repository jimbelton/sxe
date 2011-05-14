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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>     /* for PATH_MAX     on WIN32 */
#include <limits.h>     /* for PATH_MAX not on WIN32 */

#include "sxe-log.h"
#include "sxe-test.h"

/**
 * Why go to the trouble of creating an especially unique file
 * name in a special 'temp' folder?
 * The reason is that the VirtualBox (vbox) host-guest file system
 * has a bug and does not support memory mapped files with read
 * *and* write access:
 * http://www.virtualbox.org/ticket/819
 **/

void
sxe_test_get_temp_file_name(
    const char * file_stem                       , /* e.g. test-sxe-mmap-pool */
    char       * unique_path_and_file_buffer     , /* e.g. outputs win32: C:\DOCUME~1\SIMONH~1.GRE\LOCALS~1\Temp\<file_stem>-pid-<pid>.bin */
                                                   /* e.g. outputs linux:                                   /tmp/<file_stem>-pid-<pid>.bin */
    unsigned     unique_path_and_file_buffer_size,
    unsigned   * unique_path_and_file_buffer_used)
{
    int   used     = 0                               ;
    int   size     = unique_path_and_file_buffer_size;
    int   done                                       ;
#if defined(WIN32)
    DWORD UniqueId = GetCurrentThreadId()            ;
#else
    pid_t UniqueId = getpid()                        ;
#endif

    SXEE63("sxe_test_get_temp_file(file_stem=%p, unique_path_and_file_buffer=%p, unique_path_and_file_buffer_size=%u)", file_stem, unique_path_and_file_buffer, unique_path_and_file_buffer_size);

    SXEA11(PATH_MAX == unique_path_and_file_buffer_size, "ERROR: unique_path_and_file_buffer_sizes should be PATH_MAX but is %u", unique_path_and_file_buffer_size);

#ifdef _WIN32
    done = GetTempPath(size, unique_path_and_file_buffer); /* e.g. C:\TEMP\ */
    SXEA10(done != 0, "ERROR: GetTempPath() returned zero unexpectedly");
#else
    done = snprintf(&unique_path_and_file_buffer[used], size, "/tmp/");
    SXEA12(done <= size, "ERROR: snprintf() did not have enough space; given %d bytes and would have used %d bytes", size, done);
    SXEA11(done >  0   , "ERROR: snprintf() failed with result: %d", done);
#endif
    size -= done;
    used += done;

    done  = snprintf(&unique_path_and_file_buffer[used], size, "%s-pid-%d.bin", file_stem, UniqueId);
    SXEA12(done <= size, "ERROR: snprintf() did not have enough space; given %d bytes and would have used %d bytes", size, done);
    SXEA11(done >  0   , "ERROR: snprintf() failed with result: %d", done);
    size -= done;
    used += done;

    *unique_path_and_file_buffer_used = used;

    SXER61("return // unique_path_and_file_buffer=%s", unique_path_and_file_buffer);
}
