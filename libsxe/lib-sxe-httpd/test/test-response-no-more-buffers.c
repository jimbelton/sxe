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

#include "common.h"

#define TEST_WAIT           5.0
#define TEST_SEND_BYTES   640 * 1024
#define TEST_SEND_COUNT  1000
#define TEST_BUFFER_SIZE  512
#define TEST_BUFFER_COUNT  10 /* 10 times 512 < 8192 -- too few buffers */

static SXE_HTTPD httpd;

int
main(void)
{
    SXE_HTTPD_REQUEST      * request[TEST_BUFFER_COUNT];
    SXE                    * client[TEST_BUFFER_COUNT];
    tap_ev                   ev;
    SXE                    * listener;
    char                   * send_buffer;
    int                      i;

    SXEA1(send_buffer = malloc(TEST_SEND_BYTES), "Failed to allocate %u KB", TEST_SEND_BYTES);

    for (i = 0; i < TEST_SEND_BYTES; i++) {
        send_buffer[i] = 'A';
    }

    tap_plan(33, TAP_FLAG_ON_FAILURE_EXIT, NULL);
    test_sxe_register_and_init(TEST_BUFFER_COUNT * 2 + 1);

    sxe_httpd_construct(&httpd, TEST_BUFFER_COUNT + 1, TEST_BUFFER_COUNT, TEST_BUFFER_SIZE, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);

    SXEA1((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                         "sxe_httpd_listen failed");

    tap_test_case_name("connecting 10 clients");
    for (i = 0; i < TEST_BUFFER_COUNT; i++) {
        SXEA1((client[i] = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, NULL)) != NULL, "sxe_new_tcp failed");
        sxe_connect(client[i], "127.0.0.1", SXE_LOCAL_PORT(listener));

#define TEST_GET "GET / HTTP/1.1\r\n\r\n"

        is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &ev), "client_connect",      "client[%d] connected to HTTPD", i);
        TEST_SXE_SEND_LITERAL(client[i], TEST_GET, client_sent, q_client, TEST_WAIT, &ev);

        is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &ev),            "h_respond", "HTTPD[%d] respond event", i);
        request[i] = SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(ev, "request"));
    }

    tap_test_case_name("response_copy_body_data() runs out");
    {
        SXE_RETURN ret = SXE_RETURN_OK;

        sxe_httpd_response_start(request[0], 200, "OK");

        /* When we copy body data, we try to immediately send each buffer. If
         * it succeeds, we may not actually run out of buffers, because as the
         * buffers are depleted they are freed again. Eventually, though,
         * we'll fill the kernel's TCP output buffers.
         */
        for (i = 0; i < TEST_SEND_COUNT && ret == SXE_RETURN_OK; i++) {
            ret = sxe_httpd_response_copy_body_data(request[0], send_buffer, TEST_SEND_BYTES);
        }
        is(ret, SXE_RETURN_NO_UNUSED_ELEMENTS, "HTTPD: response ran out of buffers after sending %d bytes %d times", TEST_SEND_BYTES, i);

        /* NOTE: you get an assertion if you call sxe_httpd_response_end() after
         * we've run out of buffers, because the last valid buffer has been send,
         * and no other buffers are available, so request->out_buffer == NULL. Not
         * sure what to do in this case... I guess we could queue a zero-length
         * buffer? Although we're out of buffers...
         */
#if NOT_SURE_WHAT_TO_DO_HERE
        sxe_httpd_response_end(request[0], h_sent, NULL);
#endif
    }

    tap_test_case_name("response_start() runs out of buffers");
    {
        is(sxe_httpd_response_start(request[1], 200, "OK"), SXE_RETURN_NO_UNUSED_ELEMENTS, "response_start() ran out of buffers");
        sxe_httpd_close(request[1]);
        sxe_close(client[1]);
    }

    tap_test_case_name("response_simple() runs out of buffers during response_start()");
    {
        is(sxe_httpd_response_simple(request[2], NULL, NULL, 200, "OK", "", NULL), SXE_RETURN_NO_UNUSED_ELEMENTS, "response_simple() ran out of buffers");
        sxe_httpd_close(request[2]);
        sxe_close(client[2]);
    }

    /* TODO: because there are as many buffers as there are clients, we should
     * be able to test all combinations of places where we can run out of
     * buffers. We just need to ensure that all clients now call
     * response_start(), so that all have a buffer allocated. Then we can do
     * things like call sxe_httpd_response_header() with enough headers to
     * fill up the buffer and grab another one, which will fail because there
     * are no more buffers free... as long as the send does not complete
     * immediately. Perhaps it's worth mocking send() so that it always
     * returns a blocking answer?
     */

    sxe_httpd_close(request[0]);
    sxe_close(client[0]);

    return exit_status();
}
