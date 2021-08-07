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

#include "sxe-thread.h"

SXE_RETURN
sxe_thread_create(SXE_THREAD * thread, SXE_THREAD_RETURN (SXE_STDCALL * thread_main)(void *), void * user_data, unsigned options)
{
    SXE_RETURN result = SXE_RETURN_ERROR_INTERNAL;
    int        error;

    SXEE6("sxe_thread_create(thread=%p, thread_main=%p, user_data=%p, options=%u)", thread, thread_main, user_data, options);
    SXEA1(options == SXE_THREAD_OPTION_DEFAULTS, "sxe_thread_create: options must be %u", SXE_THREAD_OPTION_DEFAULTS);

#ifdef _WIN32
#   define WINDOWS_DEFAULT_SECURITY   NULL
#   define WINDOWS_DEFAULT_STACK_SIZE 0

    if ((thread = CreateThread(WINDOWS_DEFAULT_SECURITY, WINDOWS_DEFAULT_STACK_SIZE, thread_main, user_data, 0, NULL)) != NULL) {
        result = SXE_RETURN_OK;
        goto SXE_EARLY_OUT;
    }

    switch (error = sxe_socket_get_last_error()) { /* Coverage Exclusion - todo: win32 coverage */
    default: SXEL2("sxe_thread_create: Failed to create a thread: %s", sxe_socket_error_as_str(error)); break;
    }

#else /* POSIX */
#   define POSIX_DEFAULT_ATTRIBUTES   NULL

    if ((error = pthread_create(thread, POSIX_DEFAULT_ATTRIBUTES, thread_main, user_data)) == 0) {
        result = SXE_RETURN_OK;
        goto SXE_EARLY_OUT;
    }

    switch (error) {                                                                                             /* Coverage Exclusion - Failure case */
    case EAGAIN: SXEL2("sxe_thread_create: Not enough resources to create a thread");                 break;    /* Coverage Exclusion - Failure case */
    default:     SXEL2("sxe_thread_create: Unexpected error creating a thread: %s", strerror(error)); break;    /* Coverage Exclusion - Failure case */
    }

#endif

SXE_EARLY_OUT:
    SXER6("return %s", sxe_return_to_string(result));
    return result;
}
