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

#include <fcntl.h>
#include <string.h>
#include "sxe-log.h"
#include "sxe-socket.h"

#ifdef WINDOWS_NT
static WSADATA sxe_socket_wsa_data;        /* Windows Socket API state */
#endif

void
sxe_socket_init(void)
{
    SXEE80("sxe_socket_init()");

#ifdef WINDOWS_NT
    {
        int error;
        SXEA12((error = WSAStartup(MAKEWORD(2, 2), &sxe_socket_wsa_data)) == 0,
               "WSAStartup failed: (%d) %s", error, sxe_socket_error_as_str(error));
    }
#endif

    SXER80("return");
}

int
sxe_socket_set_nonblock(int sock, int flag)
{
#ifdef WINDOWS_NT
    u_long long_flag = flag;

    return ioctlsocket(sock, FIONBIO, &long_flag);
#else /* UNIX */
    int flags;

    if ((flags = fcntl(sock, F_GETFL)) < 0 || fcntl(sock, F_SETFL, (flag ? flags | O_NONBLOCK : flags & ~O_NONBLOCK)) < 0) {
        return SXE_SOCKET_ERROR_OCCURRED; /* COVERAGE EXCLUSION: cannot hit this w/o a fcntl() mockup */
    }

    return 0;
#endif
}

int
sxe_socket_get_last_error(void)
{
#ifdef WINDOWS_NT
    return WSAGetLastError();
#else
    return errno;
#endif
}

void
sxe_socket_set_last_error(int error)
{
#ifdef WINDOWS_NT
    WSASetLastError(error);
#else
    errno = error;
#endif
}

char *
sxe_socket_error_as_str(int error)
{
#ifdef WINDOWS_NT
    int         len;
    static char errmsg[512];    /* TODO: Make a thread safe version of this function */

    if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, 0, error, 0, errmsg, 511, NULL)) {
        /* if we fail, call ourself to find out why and return that error */
        return sxe_socket_error_as_str(GetLastError()); /* Coverage Exclusion - todo: win32 coverage */
    }

    errmsg[sizeof(errmsg) - 1] = '\0';
    len = strlen(errmsg);
    errmsg[len - 1] = errmsg[len - 1] == '\n' ? '\0' : errmsg[len - 1];
    errmsg[len - 2] = errmsg[len - 2] == '\r' ? '\0' : errmsg[len - 2];

    return errmsg;
#else /* UNIX */
    return strerror(error);
#endif
}

char *
sxe_socket_get_last_error_as_str(void)
{
    return sxe_socket_error_as_str(sxe_socket_get_last_error());
}
