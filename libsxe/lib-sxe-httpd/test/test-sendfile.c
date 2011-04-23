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
handle_sendfile_done(SXE_HTTPD_REQUEST *request, SXE_RETURN final, void *user_data)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(request);
    SXE_UNUSED_PARAMETER(final);
    SXEE62I("%s(final=%s)", __func__, sxe_return_to_string(final));
    close(SXE_CAST(intptr_t, user_data));
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

    plan_tests(3);

    sxe_register(4, 0);        /* http listener and connections */
    sxe_register(8, 0);        /* http clients */
    sxe_init();

    sxe_httpd_construct(&httpd, 3, 0);
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
        struct stat sb;
        unsigned expected_length;
        int fd;

        SXEA11((fd = open("sxe-httpd-proto.h", O_RDONLY)) >= 0, "Failed to open sxe-httpd-proto.h: %s", strerror(errno));
        SXEA11(fstat(fd, &sb) >= 0, "Failed to fstat sxe-httpd-proto.h: %s", strerror(errno));

        expected_length = sb.st_size + SXE_LITERAL_LENGTH("HTTP/1.1 200 OK\r\n\r\n");

        /* NOTE: sizeof readbuf just happens to be >> size of sxe-httpd-proto.h, so that's why we use it here */
        sxe_httpd_response_start(request, 200, "OK");
        sxe_httpd_response_sendfile(request, fd, sb.st_size, handle_sendfile_done, &fd);

        test_ev_wait_read(TEST_WAIT, &ev, c, "client_read", readbuf, expected_length, "client");
        /* TODO: actually test that we got the correct contents of buf */
    }

    return exit_status();
}

#endif /* Unix */
