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

#include "sxe-socket.h"
#include "tap.h"

/* What does success look like?
 */
#ifdef WINDOWS_NT
static char success[] = "The operation completed successfully.";
#else
static char success[] = "Success";
#endif

int
main(int argc, char ** argv)
{
    int sock;

    (void)argc;
    (void)argv;
    plan_tests(3);

    sxe_socket_init();

    ok   ((sock = socket(AF_INET,  SOCK_STREAM, 0)) != SXE_SOCKET_INVALID       , "Allocated a socket"             );
    is_eq(sxe_socket_get_last_error_as_str()        ,  success                  , "Last error is '%s'", success    );
    ok   (sxe_socket_set_nonblock(sock, 1)          != SXE_SOCKET_ERROR_OCCURRED, "Set socket to non blocking mode");

    sxe_socket_set_last_error(-1);

    return exit_status();
}
