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

#ifdef WINDOWS_NT
#include <Windows.h>
#endif

#define MOCK_CDECL
#define MOCK_DEF(type, scope, function, parameters) type (MOCK_ ## scope * mock_ ## function) parameters = function

/* Declarations of the mock function table.  For Windows, CRT functions have CDECL, OS (e.g. WinSock) APIs have STDCALL
 *       Return Type   CRT/OS   Function     Parameter Types
 */
MOCK_DEF(MOCK_SOCKET,  STDCALL, accept,      (MOCK_SOCKET, struct sockaddr *, MOCK_SOCKLEN_T *));
MOCK_DEF(int,          STDCALL, bind,        (MOCK_SOCKET, const struct sockaddr *, MOCK_SOCKLEN_T));
MOCK_DEF(void *,       CDECL,   calloc,      (size_t, size_t));
MOCK_DEF(int,          CDECL,   close,       (int));
MOCK_DEF(int,          STDCALL, connect,     (MOCK_SOCKET, const struct sockaddr *, MOCK_SOCKLEN_T));
MOCK_DEF(FILE *,       CDECL,   fopen,       (const char * file, const char * mode));
MOCK_DEF(int,          CDECL,   fputs,       (const char * string, FILE * file));
MOCK_DEF(int,          STDCALL, getsockopt,  (MOCK_SOCKET, int, int, MOCK_SOCKET_VOID *, MOCK_SOCKLEN_T * __restrict));
MOCK_DEF(int,          CDECL,   gettimeofday,(struct timeval * __restrict tm, __timezone_ptr_t __restrict tz));
MOCK_DEF(off_t,        CDECL,   lseek,       (int fd, off_t offset, int whence));
MOCK_DEF(int,          STDCALL, listen,      (MOCK_SOCKET, int));
MOCK_DEF(void *,       CDECL,   malloc,      (size_t));
MOCK_DEF(MOCK_SSIZE_T, STDCALL, recvfrom,    (MOCK_SOCKET, MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int, struct sockaddr *, MOCK_SOCKLEN_T *));
MOCK_DEF(MOCK_SSIZE_T, STDCALL, send,        (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int));
MOCK_DEF(MOCK_SSIZE_T, STDCALL, sendto,      (MOCK_SOCKET, const MOCK_SOCKET_VOID *, MOCK_SOCKET_SSIZE_T, int, const struct sockaddr *, MOCK_SOCKLEN_T));
MOCK_DEF(MOCK_SOCKET,  STDCALL, socket,      (int, int, int));
MOCK_DEF(MOCK_SSIZE_T, CDECL,   write,       (int, const void *, MOCK_SIZE_T));

#ifdef WINDOWS_NT
MOCK_DEF(DWORD,        STDCALL, timeGetTime, (void));
MOCK_DEF(int,          CDECL,   mkdir,       (const char * pathname));
#else
# if defined(__APPLE__)
MOCK_DEF(int         , STDCALL, sendfile,    (int, int, off_t, off_t *, struct sf_hdtr *, int));
# else
MOCK_DEF(MOCK_SSIZE_T, STDCALL, sendfile,    (int, int, off_t *, size_t));
# endif
MOCK_DEF(int,          CDECL,   mkdir,       (const char * pathname, mode_t mode));
MOCK_DEF(void,         CDECL,   openlog,     (const char * ident, int option, int facility));
MOCK_DEF(void,         CDECL,   syslog,      (int priority, const char * format, ...));
#endif

/* The following mock was removed because the function that it mocks cannot be linked statically with the debian version of glibc.
 * If you need to mock this function, put it in a separate file so that only the program that uses it requires dynamic linking.
 */
#ifdef DYNAMIC_LINKING_REQUIRED
MOCK_DEF(struct hostent *, STDCALL, gethostbyname, (const char *));
#endif
