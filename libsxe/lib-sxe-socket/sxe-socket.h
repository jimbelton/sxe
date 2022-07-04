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

#ifndef __SXE_SOCKET_H__
#define __SXE_SOCKET_H__

#ifndef WINDOWS_NT
#include <arpa/inet.h>                            /* inet (3) funtions */
#include <errno.h>
#include <netdb.h>
#endif

#include <sys/socket.h>
#ifdef __FreeBSD__
#include <netinet/in.h>
#endif


#include "mock.h"
#include "sxe-log.h"
#include "sxe-socket-proto.h"

/* - Linux sets the maximum waiting connection requests to 128 (in socket.h)
 * - Windows provides this constant as well:
 *   - winsock.h  defines it to 5
 *   - winsock2.h defines it to 0x7fffffff (!!!) which we include in sxe.c
 * - BSD uses 5 and doesn't define this constant
 */
#ifndef SOMAXCONN
#define SOMAXCONN 5
#endif

/* Use common definitions provided by mock/port layers where possible
 */
#define SXE_STDCALL                   MOCK_STDCALL
#define SXE_SOCKET                    MOCK_SOCKET
#define SXE_SOCKET_RESTRICT           __restrict
#define SXE_SOCKET_VOID               MOCK_SOCKET_VOID
#define SXE_SOCKET_SSIZE              MOCK_SOCKET_SSIZE_T
#define SXE_SOCKET_USIZE              MOCK_SIZE_T
#define SXE_SOCKLEN_T                 MOCK_SOCKLEN_T

/* Additional OS specific definitions required by the sxe-socket code and by its users
 */

#ifdef WINDOWS_NT
#define SXE_SOCKET_MSG_NOSIGNAL       0                    /* MSG_NOSIGNAL is the default under Windows     */
#define SXE_SOCKET_ERROR_OCCURRED     SOCKET_ERROR
#define SXE_SOCKET_ERROR(error)       WSA##error
#define SXE_SOCKET_FAR                FAR
#define SXE_SOCKET_INVALID            INVALID_SOCKET
#define CLOSESOCKET(sock)             closesocket(sock)    /* Deprecated: use sxe_socket_close()            */

static inline void sxe_socket_close(int sock) {closesocket(sock);}

#else  /* UNIX */

#ifdef __APPLE__
#define SXE_SOCKET_MSG_NOSIGNAL       0
#else
#define SXE_SOCKET_MSG_NOSIGNAL       MSG_NOSIGNAL
#endif

#define SXE_SOCKET_ERROR_OCCURRED     (-1)
#define SXE_SOCKET_ERROR(error)       error
#define SXE_SOCKET_FAR
#define SXE_SOCKET_INVALID            (-1)
#define WSADATA                       int
#define CLOSESOCKET(sock)             close(sock)          /* This will lead to double closes in some cases */
#define _open_osfhandle(handle, flag) (handle)

static inline void sxe_socket_close(int sock) {close(sock);}
#endif

#endif /* __SXE_SOCKET_H__ */
