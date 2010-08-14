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

#include "mock.h"

/* Declarations of the mock function table
 */
MOCK_SOCKET      (MOCK_STDCALL * mock_accept)       (MOCK_SOCKET, struct sockaddr *, MOCK_SOCKLEN_T *)
                               = accept;
int              (MOCK_STDCALL * mock_bind)         (MOCK_SOCKET, const struct sockaddr *, MOCK_SOCKLEN_T)
                               = bind;
void *           (            *  mock_calloc)       (size_t, size_t)
                               = calloc;
int              (             * mock_close)        (int)
                               = close;
int              (MOCK_STDCALL * mock_connect)      (MOCK_SOCKET, const struct sockaddr *, MOCK_SOCKLEN_T)
                               = connect;
FILE *           (             * mock_fopen)        (const char * file, const char * mode)
                               = fopen;
int              (             * mock_fputs)        (const char * string, FILE * file)
                               = fputs;
int              (MOCK_STDCALL * mock_getsockopt)   (MOCK_SOCKET, int, int, MOCK_SOCKET_VOID *, MOCK_SOCKLEN_T * __restrict)
                               = getsockopt;
int              (             * mock_gettimeofday) (struct timeval * __restrict tm, struct timezone * __restrict tz)
                               = gettimeofday;
off_t            (             * mock_lseek)        (int fd, off_t offset, int whence)
                               = lseek;
int              (MOCK_STDCALL * mock_listen)       (MOCK_SOCKET, int)
                               = listen;
MOCK_SSIZE_T     (MOCK_STDCALL * mock_recvfrom)     (MOCK_SOCKET, void *, MOCK_SOCKET_SSIZE_T, int, struct sockaddr *, MOCK_SOCKLEN_T *)
                               = recvfrom;
MOCK_SSIZE_T     (MOCK_STDCALL * mock_send)         (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int)
                               = send;
MOCK_SSIZE_T     (MOCK_STDCALL * mock_sendto)       (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int, const struct sockaddr *, MOCK_SOCKLEN_T)
                               = sendto;
MOCK_SOCKET      (MOCK_STDCALL * mock_socket)       (int, int, int)
                               = socket;
MOCK_SSIZE_T     (             * mock_write)        (int, const void *, MOCK_SIZE_T)
                               = write;

/* The following mock was removed because the function that it mocks cannot be linked statically with the debian version of glibc.
 * If you need to mock this function, put it in a separate file so that only the program that uses it requires dynamic linking.
 */
#ifdef DYNAMIC_LINKING_REQUIRED
struct hostent * (MOCK_STDCALL * mock_gethostbyname)(const char *)
                               = gethostbyname;
#endif
