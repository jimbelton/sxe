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
#include <unistd.h>   /* for getpid() on Linux */

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#if (SXE_BUF_SIZE >= 2048) || (SXE_BUF_SIZE < 1024)
#   error "Message size must be changed (SXE_BUF_SIZE < 1024 or >= 2048)"
#endif

#ifdef _WIN32
#else
static char  message_2k[2048];
static SXE * tcp_listener;            /* Emulates Apache            */
static SXE * pipe_listener;           /* Emulates the WDX service   */
#endif
static SXE * tcp_connector;           /* Emulates the endpoint      */
static SXE * tcp_accepted  = NULL;    /* Emulates Apache            */
static SXE * pipe_connector;          /* Emulates our Apache module */
static SXE * pipe_accepted = NULL;    /* Emulates the WDX service   */

static void
test_event_tcp_connected(SXE * this)
{
    SXEE6I("test_event_tcp_connected()");

    if (this != tcp_connector) {
        SXEA1I(tcp_accepted == NULL, "A TCP connection has already been accepted");
        tcp_accepted = this;
    }

    tap_ev_push(__func__, 1, "this", this);
    SXER6I("return");
}

static void
test_event_pipe_connected(SXE * this)
{
    SXEE6I("test_event_pipe_connected()");

    if (this != pipe_connector) {
        SXEA1I(pipe_accepted == NULL, "A pipe connection has already been accepted");
        pipe_accepted = this;
    }

    tap_ev_push(__func__, 1, "this", this);
    SXER6I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE6I("test_event_read(length=%d)", length);
    tap_ev_push(__func__, 2, "this", this, "length", length);
    SXER6I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE6I("test_event_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER6I("return");
}

int
main(void)
{
#ifdef _WIN32
    plan_skip_all("- skip this test program on Windows until lib-sxe can support named pipes");
    (void)test_event_tcp_connected;
    (void)test_event_pipe_connected;
    (void)test_event_read;
    (void)test_event_close;
#else
    unsigned short port;
    char           path[64];
    char           buffer[4096];
    pid_t          pid;
    tap_ev         event;
    unsigned       i;

    plan_tests(73);
    sxe_register(6, 0);
    is(sxe_init(), SXE_RETURN_OK,                                                        "sxe_init succeeded");

    tcp_listener  = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_tcp_connected, test_event_read, test_event_close);
    ok(tcp_listener != NULL,                                                             "Allocated TCP listener");
    is(sxe_listen(tcp_listener), SXE_RETURN_OK,                                          "Listening for TCP connections");
    port = SXE_LOCAL_PORT(tcp_listener);
    tcp_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0,    test_event_tcp_connected, test_event_read, test_event_close);
    ok(tcp_connector != NULL,                                                            "Allocated TCP connector");
    ok(sxe_connect(tcp_connector, "127.0.0.1", port) == SXE_RETURN_OK,                   "Initiated TCP connection");

    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_tcp_connected",      "One side of the TCP connection");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_tcp_connected",      "The other side of the TCP connection");
    ok(tcp_accepted != NULL,                                                             "TCP connection accepted");

    pid = getpid();
    snprintf(path, sizeof(path), "/tmp/sxe-test-unix-domain-socket-for-pid-%d", pid);
    pipe_listener  = sxe_new_pipe(NULL, path, test_event_pipe_connected, test_event_read, test_event_close);
    ok(pipe_listener != NULL,                                                            "Allocated pipe listener");
    is(sxe_listen(pipe_listener), SXE_RETURN_OK,                                         "Listening for pipe connections");

    pipe_connector = sxe_new_pipe(NULL, path, test_event_pipe_connected, test_event_read, test_event_close);
    ok(pipe_connector != NULL,                                                           "Allocated pipe connector");
    is(sxe_connect_pipe(pipe_connector), SXE_RETURN_OK,                                  "Connection to pipe listener initiated");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 1st pipe connected event");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 2nd pipe connected event");
    ok(pipe_accepted != NULL,                                                            "Pipe connection accepted");

    ok(recvfrom(pipe_connector->socket, buffer, sizeof(buffer), 0, NULL, NULL) < 0,      "Read on pipe connector failed");
    is(errno, EAGAIN,                                                                    "Read on pipe connector would block");

    for (i = 0; i < sizeof(message_2k) / sizeof(message_2k[0]); i++) {
        message_2k[i] = i;
    }

#   define MESSAGE1 message_2k
    is(sxe_write_pipe(pipe_connector, MESSAGE1, sizeof(MESSAGE1), tcp_accepted->socket),  SXE_RETURN_OK, "Sent a file descriptor");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got a read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Data was received on accepted pipe");
    is(SXE_BUF_USED(pipe_accepted), SXE_BUF_SIZE,                                        "Data is %u bytes", SXE_BUF_SIZE);
    ok(memcmp(SXE_BUF(pipe_accepted), MESSAGE1, SXE_BUF_SIZE) == 0,                      "It's got the right stuff");
    ok(tcp_accepted->socket  >= 0,                                                       "Apache's TCP socket is still open");
    ok(pipe_accepted->socket != tcp_accepted->socket,                                    "File descriptors differ");

    sxe_buf_clear(pipe_accepted);
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got a read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Data was received on accepted pipe");
    is(SXE_BUF_USED(pipe_accepted), sizeof(message_2k) - SXE_BUF_SIZE,                   "Its the rest of the 2K message");
    ok(memcmp(SXE_BUF(pipe_accepted), &MESSAGE1[SXE_BUF_SIZE], sizeof(message_2k) - SXE_BUF_SIZE) == 0, "The rest looks right");
    sxe_buf_clear(pipe_accepted);

    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close",      "Got a close event");
    ok(tap_ev_arg(event, "this") == pipe_connector,                                      "Pipe connector was closed");
    is(sxe_close(tcp_accepted), SXE_RETURN_OK,                                           "Closed TCP connection on Apache side");
    tcp_accepted = NULL;

#   define MESSAGE2 "Hello, world"
    is(sxe_write(tcp_connector, MESSAGE2, sizeof(MESSAGE2)), SXE_RETURN_OK,              "EP sent '" MESSAGE2 "' on TCP connection");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got another read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "More data was received on accepted pipe");
    is_eq(SXE_BUF(pipe_accepted), MESSAGE2,                                              "It's '" MESSAGE2 "'");
    sxe_buf_clear(pipe_accepted);

    is(sxe_close(tcp_connector), SXE_RETURN_OK,                                          "Closed TCP connection from end-point");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close",      "Got a close event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Pipe connector was closed");
    pipe_accepted = NULL;

    /* Test sending exactly the receive buffer size */

    tcp_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0,    test_event_tcp_connected, test_event_read, test_event_close);
    ok(tcp_connector != NULL,                                                            "Allocated TCP connector");
    ok(sxe_connect(tcp_connector, "127.0.0.1", port) == SXE_RETURN_OK,                   "Initiated TCP connection");

    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_tcp_connected",      "One side of the TCP connection");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_tcp_connected",      "The other side of the TCP connection");
    ok(tcp_accepted != NULL,                                                             "TCP connection accepted");

    pipe_connector = sxe_new_pipe(NULL, path, test_event_pipe_connected, test_event_read, test_event_close);
    ok(pipe_connector != NULL,                                                           "Allocated pipe connector");
    is(sxe_connect_pipe(pipe_connector), SXE_RETURN_OK,                                  "Connection to pipe listener initiated");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 1st pipe connected event");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 2nd pipe connected event");
    ok(pipe_accepted != NULL,                                                            "Pipe connection accepted");

    is(sxe_write_pipe(pipe_connector, MESSAGE1, SXE_BUF_SIZE, tcp_accepted->socket),  SXE_RETURN_OK, "Sent exactly max buffer");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got a read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Data was received on accepted pipe");
    is(SXE_BUF_USED(pipe_accepted), SXE_BUF_SIZE,                                        "Data is %u bytes", SXE_BUF_SIZE);
    ok(memcmp(SXE_BUF(pipe_accepted), MESSAGE1, SXE_BUF_SIZE) == 0,                      "It's got the right stuff");

    sxe_buf_clear(pipe_accepted);
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close",      "Got a close event");
    ok(tap_ev_arg(event, "this") == pipe_connector,                                      "Pipe connector was closed");
    is(sxe_close(tcp_accepted), SXE_RETURN_OK,                                           "Closed TCP connection on Apache side");
    tcp_accepted = NULL;

    is(sxe_write(tcp_connector, MESSAGE2, sizeof(MESSAGE2)), SXE_RETURN_OK,              "EP sent '" MESSAGE2 "' on TCP connection");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got another read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "More data was received on accepted pipe");
    is_eq(SXE_BUF(pipe_accepted), MESSAGE2,                                              "It's '" MESSAGE2 "'");
    sxe_buf_clear(pipe_accepted);

    is(sxe_close(tcp_connector), SXE_RETURN_OK,                                          "Closed TCP connection from end-point");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_close",      "Got a close event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Pipe connector was closed");
    pipe_accepted = NULL;

    /* Test sending exactly the receive buffer size */

    pipe_connector = sxe_new_pipe(NULL, path, test_event_pipe_connected, test_event_read, test_event_close);
    ok(pipe_connector != NULL,                                                           "Allocated pipe connector");
    is(sxe_connect_pipe(pipe_connector), SXE_RETURN_OK,                                  "Connection to pipe listener initiated");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 1st pipe connected event");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_pipe_connected",     "Got 2nd pipe connected event");
    ok(pipe_accepted != NULL,                                                            "Pipe connection accepted");

    is(sxe_write(pipe_connector, MESSAGE2, sizeof(MESSAGE2)), SXE_RETURN_OK,             "Sent '" MESSAGE2 "' on pipe connection");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got yet another read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Data was received on accepted pipe");
    is_eq(SXE_BUF(pipe_accepted), MESSAGE2,                                              "It's '" MESSAGE2 "'");
    sxe_buf_clear(pipe_accepted);

    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");
    unlink(path);
#endif

    return exit_status();
}
