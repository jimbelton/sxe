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

#ifndef __PORT_SYS_WAIT__
#define __PORT_SYS_WAIT__

#ifdef MAKE_MINGW
#else
#include <process.h>
#endif

#define WEXITSTATUS(status) (status)
#define WIFEXITED(status)   1             /* Always true on Windows   */
#define WNOHANG             0x00000001    /* Same as Linux            */
#define WTERMSIG(status)    0             /* Not supported on Windows */

/* bits/waitstatus.h */
#define __WIFSIGNALED(status) \
  (((signed char) (((status) & 0x7f) + 1) >> 1) > 0)

/* stdlib.h */
#define __WAIT_INT(status)   (*(int *) &(status))

/* sys/wait.h */
#define WIFSIGNALED(status) __WIFSIGNALED(__WAIT_INT(status))
typedef int pid_t;

pid_t waitpid(pid_t pid, int * status, int options); /* keep quiet mingw32-gcc.exe */

#endif
