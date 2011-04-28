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

static void
handle_sent(SXE_HTTPD_REQUEST *request, SXE_RETURN final, void *user_data)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(final);
    SXE_UNUSED_PARAMETER(user_data);
    SXEE62I("%s(final=%s)", __func__, sxe_return_to_string(final));
    tap_ev_push(__func__, 2, "request", request, "final", final);
    SXER60I("return");
}

static void
http_respond(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE61I("%s()", __func__);
    tap_ev_push(__func__, 1, "request", request);
    SXER60I("return");
}

static void
client_connect(SXE * this)
{
    SXEE61I("%s()", __func__);
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
client_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE62I("%s(length=%u)", __func__, length);
    tap_ev_push(__func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD httpd;
    SXE_HTTPD_REQUEST * request;
    tap_ev ev;
    SXE *listener;
    SXE *c;

    plan_tests(4);

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
        char readbuf[65536];
        SXE_BUFFER buffer, buffer2;
        unsigned expected_length;

        buffer.ptr  = "Hello, world";
        buffer.len  = strlen(buffer.ptr);
        buffer.sent = 0;
        buffer2     = buffer;

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

        expected_length = SXE_LITERAL_LENGTH("HTTP/1.1 200 OK\r\n")                                 //   17 =   17      =   17
                        + SXE_LITERAL_LENGTH("Content-Type: text/plain; charset=.UTF-8.\r\n")       // + 43 =   60      =   60
                        + SXE_LITERAL_LENGTH("Content-Length: dd\r\n")                              // + 20 =   80      =   80
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  157      =  157
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  234      =  234
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  311      =  311
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  388      =  388
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  465      =  465
                        /* end of first buffer */
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =   77      =  542
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  154      =  619
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  231      =  696
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  308      =  773
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  385      =  850
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V77 "\r\n")                              // + 77 =  462      =  927
                        + SXE_LITERAL_LENGTH(X_NAM "x: " X_V49 "\r\n")                              // + 49 =  511      =  976
                        /* end of second buffer */
                        + SXE_LITERAL_LENGTH("\r\n")                                                // +  2 =    2      =  978
                        /* end of headers - rest is body */
                        + buffer.len                                                                // + 12 =   14      =  990
                        + buffer.len                                                                // + 12 =   26      = 1002
                        + buffer.len                                                                // + 12 =   38      = 1014
                        ;

        /* NOTE: sizeof readbuf just happens to be >> size of sxe-httpd-proto.h, so that's why we use it here */
        sxe_httpd_response_start(request, 200, "OK");
        sxe_httpd_response_header(request, "Content-Type", "text/plain; charset=\"UTF-8\"", 0);
        sxe_httpd_response_content_length(request, (int)(3 * buffer.len));
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
        sxe_httpd_response_end(request, handle_sent, NULL);
        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "handle_sent", "HTTPD finished request");

        SXEL41("Expecting to read %u bytes", expected_length);
        test_ev_wait_read(TEST_WAIT, &ev, c, "client_read", readbuf, expected_length, "client");
        /* TODO: actually test that we got the correct contents of buf */
    }

    return exit_status();
}
