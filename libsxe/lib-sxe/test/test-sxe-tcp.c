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

#include <string.h>
#include <stdlib.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT 2    /* Maximum seconds to wait for an event */

static char my_giant_buffer[] = \
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    "1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890";

static void
test_event_connected(SXE * this)
{
    SXEE60I("test_event_connected()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE61I("test_event_read(length=%d)", length);
    tap_ev_push(__func__, 3, "this", this, "length", length, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)));
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE60I("test_event_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

int
main(int argc, char *argv[])
{
    SXE_RETURN   result;
    int          port;
    SXE        * listener;
    SXE        * connectee;
    SXE        * connector;
    tap_ev       event;
    const char * buf;

    plan_tests(70);
    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    port = 9090;
    sxe_register(3, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    /* connect_test
     */

    listener  = sxe_new_tcp(NULL, "INADDR_ANY", port, test_event_connected, test_event_read, test_event_close);
    connector = sxe_new_tcp(NULL, "INADDR_ANY", 0,    test_event_connected, test_event_read, test_event_close);

    is(sxe_listen(listener),                       SXE_RETURN_OK,                       "creation of listener succeeded");
    is(sxe_connect(connector, "127.0.0.1", port),  SXE_RETURN_OK,                       "connection to listener initiated");
    is(sxe_connect(connector, "127.0.0.1", port),  SXE_RETURN_ERROR_ALREADY_CONNECTED,  "reconnection on connecting SXE fails");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 1st connected event");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 2nd connected event");

    /* Write tests
     */
    is(result = sxe_write(connector, "HELO", 4), SXE_RETURN_OK,                         "wrote 'HELO' to connector");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",      "got a read event");
    connectee = SXE_CAST_NOCONST(SXE *, tap_ev_arg(event, "this"));
    is(tap_ev_arg(event, "length"), 4,                                                  "read a four byte value");
    is(SXE_BUF_USED(connectee),     4,                                                  "four bytes in buffer");
    is_strncmp(SXE_BUF(connectee), "HELO", 4,                                           "'HELO' in buffer");

    is(result = sxe_write(connector, ", WORLD", 7), SXE_RETURN_OK,                      "wrote ', WORLD' to connector");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",      "got a read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "on the connectee");
    is(tap_ev_arg(event, "length"), 7,                                                  "read a 7 byte value");
    is(SXE_BUF_USED(connectee),     11,                                                 "eleven bytes in buffer");
    is_strncmp(SXE_BUF(connectee), "HELO, WORLD", 11,                                   "'HELO, WORLD' in buffer");
    SXE_BUF_CLEAR(connectee);    /* To consume the data */

    is(result = sxe_write(connector, "GOODBYE", 7), SXE_RETURN_OK,                      "wrote 'GOODBYE' to connector");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",      "got a read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "on the connectee");
    is(tap_ev_arg(event, "length"), 7,                                                  "read a 7 byte value");
    is(SXE_BUF_USED(connectee),     7,                                                  "seven bytes in buffer");
    is_strncmp(SXE_BUF(connectee), "GOODBYE", 7,                                        "'GOODBYE' in buffer");
    SXE_BUF_CLEAR(connectee);    /* To consume the data */

    /* While we have three connections open, we'll try to create a fourth,
     * which we expect to fail because we only allowed for three with calls to
     * sxe_register() prior to calling sxe_init()
     */
    is(sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close), NULL,
                                                                                        "SXE new failed: out of slots");

    /* Close test
     */
    is(sxe_close(connectee), SXE_RETURN_OK, "Close of connectee succeeded");
    connectee = NULL;
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close",     "got a close event");
    is(tap_ev_arg(event, "this"), connector,                                            "on the connector");

    /* Write to a peer who has closed
     */
    is(sxe_write(connector, "HELLO AGAIN", 11), SXE_RETURN_ERROR_NO_CONNECTION,         "can't write to a closed SXE object");
    test_process_all_libev_events();

    /* Test one-shot listeners
     */
    port++;
    ok((listener = sxe_new_tcp(NULL, "127.0.0.1", port, test_event_connected, test_event_read, test_event_close)) != NULL,
                                                                                        "reallocated listener");
    ok((connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close)) != NULL,
                                                                                        "re-reallocated connector");
    sxe_set_listen_backlog(1024);
    is(sxe_listen_oneshot(listener), SXE_RETURN_OK,                                     "created one-shot listener");
    is(sxe_connect(connector, "127.0.0.1", port), SXE_RETURN_OK,                        "connected to one-shot listener");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 3rd connected event");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected", "got 4th connected event");

    /* Test consume and resume functions
     */
    is(sxe_write(connector, "thi", SXE_LITERAL_LENGTH("thi")), SXE_RETURN_OK,           "Sent 'thi'");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got a read event");
    is(tap_ev_arg(event, "length"), SXE_LITERAL_LENGTH("thi"),                          "Got three characters");
    is_strncmp(tap_ev_arg(event, "buf"), "thi", 3,                                      "Got 'thi'");
    is(sxe_write(connector, "s test gets", SXE_LITERAL_LENGTH("s test gets")), SXE_RETURN_OK, "Sent 's test gets'");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got a read event");
    is(tap_ev_arg(event, "length"), SXE_LITERAL_LENGTH("s test gets"),                  "Got %u characters", (unsigned)SXE_LITERAL_LENGTH("s test gets"));
    connectee = SXE_CAST_NOCONST(SXE *, tap_ev_arg(event, "this"));
    buf       = tap_ev_arg(event, "buf");
    is(sxe_strnchr(buf, ' ', SXE_BUF_USED(connectee)), &buf[4],                         "Found a ' ' at offset 4");
    sxe_buf_consume(connectee, 5);

    is(sxe_write(connector, " one word at a time", SXE_LITERAL_LENGTH(" one word at a time")), SXE_RETURN_OK, "Sent ' one word at a time'");
    is(sxe_write(connectee, "boo!", 4), SXE_RETURN_OK,                                  "Sent a message back to the connector");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got a read event");
    is(tap_ev_arg(event, "this"), connector,                                            "Read event was on the connector");
    sxe_buf_resume(connectee, SXE_BUF_RESUME_IMMEDIATE);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), SXE_LITERAL_LENGTH("test gets one word at a time"), "Length read is '%u'", (unsigned)SXE_LITERAL_LENGTH("test gets one word at a time"));
    is_strncmp(tap_ev_arg(event, "buf"), "test ", 5,                                    "Second word is in the buffer");
    is(SXE_BUF_USED(connectee), SXE_LITERAL_LENGTH("test gets one word at a time"),     "Length unconsumed is '%u'", (unsigned)SXE_LITERAL_LENGTH("test gets one word at a time"));
    sxe_buf_consume(connectee, 5);

    sxe_buf_resume(connectee, SXE_BUF_RESUME_IMMEDIATE);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), SXE_LITERAL_LENGTH("gets one word at a time"),      "Length read is '%u'", (unsigned)SXE_LITERAL_LENGTH("gets one word at a time"));
    is(sxe_write(connector, my_giant_buffer, SXE_BUF_SIZE - SXE_LITERAL_LENGTH("this test gets one word at a time")), SXE_RETURN_OK,
                                                                                        "Filled the buffer");
    sxe_buf_consume(connectee, 5);
    sxe_buf_resume(connectee, SXE_BUF_RESUME_IMMEDIATE);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), SXE_LITERAL_LENGTH("one word at a time"),           "Length read is '%u'", (unsigned)SXE_LITERAL_LENGTH("one word at a time"));

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), SXE_BUF_SIZE - SXE_LITERAL_LENGTH("this test gets one word at a time"), "Length read is '%u'",
                  (unsigned)(SXE_BUF_SIZE - SXE_LITERAL_LENGTH("this test gets one word at a time")));
    sxe_buf_consume(connectee, SXE_BUF_USED(connectee));    /* Cover the case of consuming the whole buffer */
    sxe_buf_resume(connectee, SXE_BUF_RESUME_WHEN_MORE_DATA);

    is(sxe_write(connector, my_giant_buffer, sizeof my_giant_buffer), SXE_RETURN_OK,    "Wrote %u bytes to connector", (unsigned)sizeof(my_giant_buffer));
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), SXE_BUF_SIZE,                                       "Length read is '%u'", SXE_BUF_SIZE);
    sxe_buf_consume(connectee, 100); /* Consume some of the buffer, leaving room for more data. */
    sxe_buf_resume(connectee, SXE_BUF_RESUME_WHEN_MORE_DATA);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read",            "Got another read event");
    is(tap_ev_arg(event, "this"), connectee,                                            "Read event was on the connectee");
    is(tap_ev_arg(event, "length"), 1,                                                  "Length read is 1");
    is(SXE_BUF_USED(connectee), sizeof(my_giant_buffer) - 100,                          "SXE buffer length is %u", (unsigned)sizeof(my_giant_buffer) - 100);

    sxe_dump(connector);    /* Just for coverage */
    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");
    return exit_status();
}
