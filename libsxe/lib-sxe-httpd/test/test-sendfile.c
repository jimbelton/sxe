/* Copyright 2010 Sophos Limited. All rights reserved. Sophos is a registered
 * trademark of Sophos Limited.
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

#include <string.h>
#include <errno.h>

#include "tap.h"
#include "sxe-httpd.h"
#include "sxe-test.h"
#include "sxe-util.h"

#define TEST_WAIT 5.0

#ifdef WINDOWS_NT

int
main(void)
{
    plan_skip_all("No sendfile on windows");
    return 0;
}

#else

static void
handle_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN final, void *user_data)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(final);
    SXE_UNUSED_PARAMETER(user_data);
    SXEE6I("%s(final=%s)", __func__, sxe_return_to_string(final));
    tap_ev_push(__func__, 2, "request", request, "final", final);
    SXER6I("return");
}

static void
http_respond(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s()", __func__);
    tap_ev_push(__func__, 1, "request", request);
    SXER6I("return");
}

static void
client_connect(SXE * this)
{
    SXEE6I("%s()", __func__);
    tap_ev_push(__func__, 1, "this", this);
    SXER6I("return");
}

static void
client_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE6I("%s(length=%u)", __func__, length);
    tap_ev_push(__func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER6I("return");
}

int
main(void)
{
    SXE_HTTPD httpd;
    SXE_HTTPD_REQUEST * request;
    tap_ev ev;
    SXE *listener;
    SXE *c;

    plan_tests(5);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    sxe_init();

    sxe_httpd_construct(&httpd, 3, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, http_respond);

    listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0);

    c = sxe_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL);
    sxe_connect(c, "127.0.0.1", SXE_LOCAL_PORT(listener));

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "client_connect",                   "Client connected to HTTPD");
    SXE_WRITE_LITERAL(c, "GET /file HTTP/1.1\r\n\r\n");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "http_respond",                     "HTTPD ready to respond");
    request = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));

    {
        char        readbuf[65536];
        SXE_BUFFER  buffer;
        SXE_BUFFER  buffer2;
        SXE_BUFFER  buffer3;
        SXE_BUFFER  buffer4;
        struct stat sb;
        unsigned    expected_length;
        int         fd;

        SXEA1((fd = open("sxe-httpd-proto.h", O_RDONLY)) >= 0, "Failed to open sxe-httpd-proto.h: %s", strerror(errno));
        SXEA1(fstat(fd, &sb) >= 0, "Failed to fstat sxe-httpd-proto.h: %s", strerror(errno));

        sxe_buffer_construct_const(&buffer, "Hello, world", SXE_LITERAL_LENGTH("Hello, world"));
        buffer2     = buffer;
        buffer3     = buffer;

        /* NOTE: these numbers are carefully designed so that we'll hit the
         * following conditions:
         *
         * 1. Ensure that we write > 512 bytes of headers, so that we'll test
         *    the case of failing to append to the first out_buffer_list
         *    entry.
         *
         * 2. Ensure that we *then* write 511 bytes of additional headers, so
         *    that the attempt to append the final "\r\n" requires another
         *    buffer.
         */
#define X_NAM "X-Header"
#define X_V77 "abcdefghijklmnopqrstuvwxyz-0123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define X_V49 "123456789-ABCDEFGHIJKLMNOPQRSTUVWXYZ"
#define X_V770 X_V77 X_V77 X_V77 X_V77 X_V77 X_V77 X_V77 X_V77 X_V77 X_V77
#define X_V7700 X_V770 X_V770 X_V770 X_V770 X_V770 X_V770 X_V770 X_V770 X_V770 X_V770
#define X_V30800 X_V770 X_V770 X_V770 X_V770

        sxe_buffer_construct_const(&buffer4, X_V30800, SXE_LITERAL_LENGTH(X_V30800));

        expected_length = SXE_LITERAL_LENGTH("HTTP/1.1 200 OK\r\n")                                 //   17 =   17      =   17
                        + SXE_LITERAL_LENGTH("Content-Type: text/plain; charset=.UTF-8.\r\n")       // + 43 =   60      =   60
                        + SXE_LITERAL_LENGTH("Content-Length: dddd\r\n")                            // + 22 =   82      =   82
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  159      =  159
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  236      =  236
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  313      =  313
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  390      =  390
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  467      =  467
                        /* end of first buffer */
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =   77      =  544
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  154      =  621
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  231      =  698
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  308      =  775
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  385      =  852
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  462      =  929
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V49 "\r\n")                              // + 49 =  511      =  978
                        /* end of second buffer */
                        + SXE_LITERAL_LENGTH("\r\n")                                                // +  2 =    2      =  980
                        /* end of headers - rest is body */
                        + sxe_buffer_length(&buffer)                                                // + 12 =   12      =  992
                        + sxe_buffer_length(&buffer)                                                // + 12 =   12      = 1004
                        + sxe_buffer_length(&buffer)                                                // + 12 =   12      = 1016
                        + sxe_buffer_length(&buffer4)
                        /* remainder is file */
                        + sb.st_size
                        + sxe_buffer_length(&buffer)
                        + sxe_buffer_length(&buffer)
                        ;

        /* NOTE: sizeof readbuf just happens to be >> size of sxe-httpd-proto.h, so that's why we use it here */
        sxe_httpd_response_start(request, 200, "OK");
        sxe_httpd_response_header(request, "Content-Type", "text/plain; charset=\"UTF-8\"", 0);
        sxe_httpd_response_content_length(request, (int)sb.st_size);
        sxe_httpd_response_header(request, X_NAM "1", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "2", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "3", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "4", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "5", X_V77, 0);
        /* end of first buffer */
        sxe_httpd_response_header(request, X_NAM "6", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "7", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "8", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "9", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "A", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "B", X_V77, 0);
        sxe_httpd_response_header(request, X_NAM "C", X_V49, 0);
        sxe_httpd_response_copy_body_data(request, "Hello, world", 0);
        sxe_httpd_response_add_body_buffer(request, &buffer);
        sxe_httpd_response_add_raw_buffer(request, &buffer2);
        sxe_httpd_response_add_raw_buffer(request, &buffer4);
        sxe_httpd_response_sendfile(request, fd, sb.st_size, handle_sent, NULL);
        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "handle_sent", "HTTPD finished sending");
        close(fd);
        sxe_httpd_response_copy_body_data(request, "Hello, world", 0);
        sxe_httpd_response_add_body_buffer(request, &buffer3);
        sxe_httpd_response_end(request, handle_sent, NULL);
        test_process_all_libev_events();
        ok(tap_ev_shift_next("handle_sent") != NULL, "HTTPD finished request");

        test_ev_wait_read(TEST_WAIT, &ev, c, "client_read", readbuf, expected_length, "client");
        /* TODO: actually test that we got the correct contents of buf */
    }

    return exit_status();
}

#endif /* Unix */
