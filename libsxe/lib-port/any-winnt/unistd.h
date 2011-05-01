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

/* Emulate unistd.h on Windows */

#ifndef __SXE_UNISTD_H
#define __SXE_UNISTD_H

#include <fcntl.h>
#include <io.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <winsock2.h> /* For MAX_PATH */

#define _CRT_NONSTDC_NO_DEPRECATE 1                     /* Don't complain about POSIX functions                             */
#define __func__                  __FUNCTION__          /* Emulate __func__ magic variable with __FUNCTION__ magic constant */

#define PIPE_BUF      4096

/* If not using mingw (minimal GNU for Windows)
 */
#ifndef MAKE_MINGW
#   define PATH_MAX      MAX_PATH
#   define STDERR_FILENO fileno(stderr)
#   define STDOUT_FILENO fileno(stdout)
#   define __thread      __declspec(thread) /* Storage specifier for thread local storage */
#else
#   include <limits.h>    /* For PATH_MAX */
#endif /* !MAKE_MINGW */

/* Emulate GCC builtin atomic operations with Windows equivalents; Note that MINGW doesn't support the GCC builtins yet
 */
#define __sync_val_compare_and_swap(dest, comperand, exchange) InterlockedCompareExchangePointer(dest, exchange, comperand)
#define __sync_add_and_fetch(       addend, value)             InterlockedExchangeAdd(           addend, value)

#define ftruncate(fd, size)     _chsize((fd), (size))
#define sleep(t)                Sleep((t) * 1000)
#define usleep(t)               Sleep((t) / 1000)

/* Signal emulation - Windows seems to be consistent with Linux for the signal numbers it defines
 */
#define EINTR         4      /* Interrupted system call */
#define SIGKILL       9
#define SIGHUP        1

/* File locking is stubbed out: caveat emptor!
 */
#define flockfile(file)
#define funlockfile(file)

/* UNIX work-alikes
 */
#define getpid()                 _getpid()
#define pclose(stream)           _pclose(stream)
#define pipe(phandles)           _pipe((phandles), PIPE_BUF, _O_BINARY)
#define popen(command, modes)    _popen((command), (modes))
#define snprintf                 _snprintf
#define strncasecmp(s1, s2, n)   _strnicmp((s1), (s2), (n))
#define unsetenv(variable)       (SetEnvironmentVariable((variable), NULL) ? 0 : -1)

/* These should be in <sys/types.h>, but they're not. Thanks, Bill :)
 */
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
typedef int     pid_t;

int vasprintf(char **strp, const char *fmt, va_list ap);
struct tm * gmtime_r(const time_t * timep, struct tm * result);   /* Should be in time.h, but it's not */

#endif
