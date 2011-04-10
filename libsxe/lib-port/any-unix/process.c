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

/* Emulate Windows spawn and cwait functions on UNIX */

#include <assert.h>
#include <errno.h>
#include <process.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

intptr_t
spawnl(int mode, const char * command, const char * arg0, ...)
{
    int pid;
    int i, nargs;
    const char **argv;

    assert(mode == P_NOWAIT);    /* Only the P_NOWAIT mode is supported */

    /* Count the number of arguments */
    {
        va_list va;

        nargs = 2; /* arg0, NULL */
        va_start(va, arg0);
        while (va_arg(va, const char *) != 0)
            nargs++;
        va_end(va);
    }

    /* Construct an argv array */
    {
        va_list va;
        const char *arg;

        argv = alloca(nargs * sizeof(const char *));
        assert(argv);

        argv[0] = arg0;
        va_start(va, arg0);
        for (i = 1; (arg = va_arg(va, const char *)) != 0; i++) {
            argv[i] = arg;
        }
        va_end(va);
        argv[i] = NULL;
    }

    /* On error or if parent, return -1 or pid
     */
    if ((pid = fork()) != 0) {
        return (intptr_t)pid;
    }

    execv(command, (char * const *)(intptr_t)argv);
    fprintf(stderr, "Failed to execute %s: %s", command, strerror(errno));
    exit(1);
    return (intptr_t)-1;    /* Can't happen */
}

intptr_t
cwait(int * status, intptr_t process_id, int action)
{
    assert(action == WAIT_CHILD);    /* Only the WAIT_CHILD action is supported */

    return (intptr_t)waitpid((pid_t)process_id, status, 0);
}
