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
static SXE * pipe_listener;           /* Emulates the WDX service   */
#endif
static SXE * client_connected = NULL; /* Emulates Apache            */
static SXE * pipe_connector;          /* Emulates our Apache module */
static SXE * pipe_accepted = NULL;    /* Emulates the WDX service   */

static void
test_event_pipe_connected(SXE * this)
{
    SXEE60I("test_event_pipe_connected()");

    if (this != pipe_connector) {
        SXEA10I(pipe_accepted == NULL, "A pipe connection has already been accepted");
        pipe_accepted = this;
    }

    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

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
    tap_ev_push(__func__, 2, "this", this, "length", length);
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
main(void)
{
#ifdef _WIN32
    plan_skip_all("- skip this test program on Windows until lib-sxe can support named pipes");
    (void)test_event_pipe_connected;
    (void)test_event_connected;
    (void)test_event_read;
    (void)test_event_close;
    (void)client_connected;
#else
    char           path[64];
    char           buffer[4096];
    pid_t          pid;
    tap_ev         event;
    int            paired_socket;
    unsigned       i;

    plan_tests(25);
    sxe_register(6, 0);
    is(sxe_init(), SXE_RETURN_OK,                                                        "sxe_init succeeded");

    client_connected  = sxe_new_socketpair(NULL, &paired_socket, test_event_connected, test_event_read, test_event_close);
    ok(client_connected != NULL,                                                         "Allocated connected socket");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_connected",  "Got a connected event");
    ok(tap_ev_arg(event, "this") == client_connected,                                    "Got a connected event from the socketpair");

    /* Try writing a few bytes to the other descriptor, and see whether we get
     * a read event on the connected SXE! */
    SXEA10(write(paired_socket, "Hello", 5) == 5,                                        "Failed to write even 5 lousy bytes to socket");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got a read event from the socketpair");
    ok(tap_ev_arg(event, "this") == client_connected,                                    "Data was received on connected socketpair");
    is(SXE_BUF_USED(client_connected), SXE_LITERAL_LENGTH("Hello"),                      "Data is %u bytes", (unsigned)SXE_LITERAL_LENGTH("Hello"));
    ok(memcmp(SXE_BUF(client_connected), "Hello", SXE_LITERAL_LENGTH("Hello")) == 0,     "It's got the right stuff");

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
    is(sxe_write_pipe(pipe_connector, MESSAGE1, sizeof(MESSAGE1), paired_socket), SXE_RETURN_OK, "Sent a file descriptor");
    is_eq(tap_ev_identifier(event = test_tap_ev_shift_wait(2)), "test_event_read",       "Got a read event");
    ok(tap_ev_arg(event, "this") == pipe_accepted,                                       "Data was received on accepted pipe");
    is(SXE_BUF_USED(pipe_accepted), SXE_BUF_SIZE,                                        "Data is %u bytes", SXE_BUF_SIZE);
    ok(memcmp(SXE_BUF(pipe_accepted), MESSAGE1, SXE_BUF_SIZE) == 0,                      "It's got the right stuff");
    ok(client_connected->socket  >= 0,                                                   "Apache's TCP socket is still open");
    ok(pipe_accepted->socket != client_connected->socket,                                "File descriptors differ");

    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");
    unlink(path);
#endif

    return exit_status();
}
