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

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_PORT 9191

static tap_ev_queue client_queue;
static SXE        * listener;
static SXE        * connectee = NULL;
static SXE        * first_connector;
static SXE        * second_connector;
static SXE        * third_connector;

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
    SXEE62I("test_event_read(this->socket=%d, length=%d)", this->socket, length);
    tap_ev_push(__func__, 3, "this", this, "length", length, "buf_used", (size_t)SXE_BUF_USED(this));
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE61I("test_event_close(this->socket=%d)", this->socket);
    tap_ev_push(__func__, 2, "this", this, "buf_used", (size_t)SXE_BUF_USED(this));
    SXER60I("return");
}

static void
test_event_client_connected(SXE * this)
{
    SXEE60I("test_event_client_connected()");
    tap_ev_queue_push(client_queue, __func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_client_read(SXE * this, int length)
{
    SXEE62I("test_event_client_read(this->socket=%d, length=%d)", this->socket, length);
    tap_ev_queue_push(client_queue, __func__, 3, "this", this, "length", length, "buf_used", (size_t)SXE_BUF_USED(this));
    SXER60I("return");
}

static void
test_event_client_close(SXE * this)
{
    SXEE61I("test_event_client_close(this->socket=%d)", this->socket);
    tap_ev_queue_push(client_queue, __func__, 2, "this", this, "buf_used", SXE_BUF_USED(this));
    SXER60I("return");
}

static MOCK_SOCKET SXE_STDCALL
test_mock_accept(SXE_SOCKET sock, struct sockaddr * peer_addr, MOCK_SOCKLEN_T * peer_addr_size)
{
    SXEE63("test_mock_accept(sock=%d,peer_addr=%p,peer_addr_size=%p)", sock, peer_addr, peer_addr_size);
    SXEL65("tap_ev_push(%s=%d, sock=%p, peer_addr=%p, peer_addr_size=%d);", __func__, 3, sock, tap_dup(peer_addr, *peer_addr_size), *peer_addr_size);
    tap_ev_push(__func__, 3, "sock", sock, "peer_addr", tap_dup(peer_addr, *peer_addr_size), "peer_addr_size", *peer_addr_size);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EINVAL));
    MOCK_SET_HOOK(accept, accept);
    SXER60("return -1 // error=EINVAL");
    return -1;
}

static SXE_SOCKET SXE_STDCALL
test_mock_socket(int domain, int type, int protocol)
{
    SXEE63("test_mock_socket(domain=%d,type=%d,protocol=%d)", domain, type, protocol);
    SXE_UNUSED_PARAMETER(domain);
    SXE_UNUSED_PARAMETER(type);
    SXE_UNUSED_PARAMETER(protocol);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EMFILE));
    SXER60("return -1 // error=EMFILE");
    return -1;
}

static int SXE_STDCALL
test_mock_listen(SXE_SOCKET sock, int backlog)
{
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(backlog);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EBADF));
    return -1;
}

static int SXE_STDCALL
test_mock_bind_efault(SXE_SOCKET sock, const struct sockaddr* addr, SXE_SOCKLEN_T len)
{
    SXEE90("test_mock_bind_efault()");
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(addr);
    SXE_UNUSED_PARAMETER(len);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EFAULT));
    SXER90("return -1 (last_error=EFAULT)");
    return -1;
}

static int SXE_STDCALL
test_mock_bind_eaddrinuse(SXE_SOCKET sock, const struct sockaddr* addr, SXE_SOCKLEN_T len)
{
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(addr);
    SXE_UNUSED_PARAMETER(len);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EADDRINUSE));
    return -1;
}

static int SXE_STDCALL
test_mock_bind_fake_success(SXE_SOCKET sock, const struct sockaddr* addr, SXE_SOCKLEN_T len)
{
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(addr);
    SXE_UNUSED_PARAMETER(len);
    return 0;
}

static int SXE_STDCALL
test_mock_connect(SXE_SOCKET sock, const struct sockaddr* addr, SXE_SOCKLEN_T len)
{
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(addr);
    SXE_UNUSED_PARAMETER(len);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EFAULT));
    return -1;
}

static int SXE_STDCALL
test_mock_getsockopt(SXE_SOCKET sock, int level, int optname, SXE_SOCKET_VOID * optval, SXE_SOCKLEN_T * SXE_SOCKET_RESTRICT optlen)
{
    SXEE65("test_mock_getsockopt(sock=%d, level=%d, optname=%d, optval=%p, optlen=%p)", sock, level, optname, optval, optlen);
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(level);
    SXE_UNUSED_PARAMETER(optname);
    SXE_UNUSED_PARAMETER(optval);
    SXE_UNUSED_PARAMETER(optlen);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EBADF));
    SXER60("return -1");
    return -1;
}

static int test_mock_send_error = 0;

static MOCK_SSIZE_T SXE_STDCALL
test_mock_send(SXE_SOCKET sock, const MOCK_SOCKET_VOID *buf, MOCK_SOCKET_SSIZE_T count, int flags)
{
    SXEE64("test_mock_send(sock=%d, buf=%p, count=%d, flags=%d)", sock, buf, count, flags);
    SXEL66("tap_ev_push(%s=%d, sock=%p, buf=%p, count=%d, flags=%d);", __func__, 4, sock, tap_dup(buf, count), count, flags);
    tap_ev_push(__func__, 4, "sock", sock, "buf", tap_dup(buf, count), "count", count, "flags", flags);
    sxe_socket_set_last_error(test_mock_send_error);
    SXER60("return -1");
    return -1;
}

int
main(void)
{
    tap_ev   ev;

    client_queue = tap_ev_queue_new();
    plan_tests(71);

     /* Initialization failure case.
     */
    ok(sxe_fini() == SXE_RETURN_ERROR_INTERNAL,                              "sxe_fini(): Failed as expected - won't work unless init was successful");
    sxe_register(1, 0); /* first plugin registers */

    /* Initialize for remaining tests. Add another 2 objects for a total of 3:
     *  - 1 listener
     *  - 2 connectors
     *  - 1 connectee
     */
    sxe_register(3, 4);
    is(sxe_init(),                  SXE_RETURN_OK,                           "sxe_init(): Initialization succeeded");

    /* Create a listener and test listen failure cases.
     */
    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close);

    MOCK_SET_HOOK(socket, test_mock_socket);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,               "sxe_listen(): Can't listen on SXE object when socket() fails");
    MOCK_SET_HOOK(socket, socket);

    MOCK_SET_HOOK(bind, test_mock_bind_eaddrinuse);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_ADDRESS_IN_USE,         "sxe_listen(): Can't listen on SXE object when bind() fails");
    MOCK_SET_HOOK(bind, bind);
    is(sxe_socket_get_last_error(), SXE_SOCKET_ERROR(EADDRINUSE),            "sxe_listen(): As expected, bind failed due to address already being in use");

    MOCK_SET_HOOK(listen, test_mock_listen);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,               "sxe_listen(): Can't listen on SXE object when listen() fails");
    MOCK_SET_HOOK(listen, listen);

    is(sxe_listen(listener),        SXE_RETURN_OK,                           "sxe_listen(): Creation of listener succeeded");
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,               "sxe_listen(): Double listen on listener fails");

    /* Accept failure cases. Allocate first_connector, and test.
     */
    first_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    ok(first_connector != NULL,                                              "1st connector: Allocated first connector");

    MOCK_SET_HOOK(accept, test_mock_accept);
    is(sxe_connect(first_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_OK,  "1st connector: Initiated connection on first connector");
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "1st connector: Got first client event");
    is_eq(tap_ev_identifier(ev),           "test_event_client_connected",    "1st connector: First event is SXE client connected");

    ok((ev = test_tap_ev_shift_wait(2)),                                     "1st connector: Got first server event");
    is_eq(tap_ev_identifier(ev),           "test_mock_accept",               "1st connector: First server event is accept() system call");
    /* MOCK_SET_HOOK(accept, accept); <-- note: moved into test_mock_accept() otherwise race condition! */
    ok((ev = test_tap_ev_shift_wait(2)) != NULL,                             "1st connector: Got another server event");
    is_eq(tap_ev_identifier(ev),           "test_event_connected",           "1st connector: Second server event is SXE server connected");

    is(tap_ev_length(),                   0,                                 "1st connector: No more client events for now");
    is(tap_ev_queue_length(client_queue), 0,                                 "1st connector: No more client events for now");

    second_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    ok(second_connector != NULL,                                             "2nd connector: Allocated second connector");
    is(sxe_connect(second_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_OK, "2nd connector: Initiated connection on second connector");
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "2nd connector: Got first client event");
    is_eq(tap_ev_identifier(ev),     "test_event_client_connected",          "2nd connector: First event is client connected");
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "2nd connector: Got second client event");
    is_eq(tap_ev_identifier(ev),     "test_event_client_close",              "2nd connector: Second event is client close");
    is(tap_ev_arg(ev, "this"),       second_connector,                       "2nd connector: Second connector closed");
    is(SXE_CAST_NOCONST(unsigned, tap_ev_arg(ev, "buf_used")), 0,            "2nd connector: 0 bytes in receive buffer");

    /* Close and reallocate connections.
     */
    sxe_close(first_connector);
    first_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    ok(first_connector != NULL,                                              "Reallocate: Reallocated first connector");
    ok((ev = test_tap_ev_shift_wait(2))   != NULL,                           "Reallocate: Got another event");
    is_eq(tap_ev_identifier(ev), "test_event_close",                         "Reallocate: It's a server close event");
    ok(tap_ev_arg(ev, "this") !=  first_connector,                           "Reallocate: First connection close indication");
    is(tap_ev_arg(ev, "buf_used"), 0,                                        "Reallocate: 0 bytes in receive buffer");

    /* Third connector uses a specific port so sxe will bind() the socket and we can test bind() failures */
    third_connector = sxe_new_tcp(NULL, "127.0.0.1", TEST_PORT, test_event_client_connected, test_event_client_read, test_event_client_close);
    ok(third_connector != NULL,                                              "Reallocate: allocated third connector");

    /* Test connection failures. */

    MOCK_SET_HOOK(socket, test_mock_socket);
    is(sxe_connect(first_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_ERROR_INTERNAL,
                                                                             "Connection failure: Can't connect to listener when socket() fails");
    MOCK_SET_HOOK(socket, socket);


    MOCK_SET_HOOK(bind, test_mock_bind_efault);
    is(sxe_connect(third_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_ERROR_INTERNAL,
                                                                             "Connection failure: Can't connect to listener when bind() fails");

    MOCK_SET_HOOK(bind, test_mock_bind_eaddrinuse);
    is(sxe_connect(third_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_ERROR_ADDRESS_IN_USE,
                                                                             "Connection failure: Can't connect to listener when address is in use");
    MOCK_SET_HOOK(bind, test_mock_bind_fake_success);

    MOCK_SET_HOOK(connect, test_mock_connect);
    is(sxe_connect(third_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_ERROR_INTERNAL,
                                                                             "Connection failure: Can't connect to listener when connect() fails");
    MOCK_SET_HOOK(connect, connect);
    MOCK_SET_HOOK(bind,    bind);
    sxe_close(listener);

    is(sxe_connect(first_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_OK,  "Connection failure: Reconnecting to listener when listener is closed");
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "Connection failure: Got another event");
    is_eq(tap_ev_identifier(ev), "test_event_client_close",                  "Connection failure: It's a client close event");
    is(tap_ev_arg(ev, "this"), first_connector,                              "Connection failure: third connection close indication" );
    is(SXE_CAST_NOCONST(unsigned, tap_ev_arg(ev, "buf_used")), 0,            "Connection failure: 0 bytes in receive buffer");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", SXE_LOCAL_PORT(listener), test_event_connected, test_event_read, test_event_close);
    is(sxe_listen(listener), SXE_RETURN_OK,                                  "Reconnect failure: Recreated listener");
    first_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    is(sxe_connect(first_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_OK,  "Reconnect failure: Reconnecting to listener to test getsockopt failure");
    MOCK_SET_HOOK(getsockopt, test_mock_getsockopt);
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "Reconnect failure: Got a client event");
    is_eq(tap_ev_identifier(ev), "test_event_client_close",                  "Reconnect failure: It's a client close event");
    is(tap_ev_arg(ev, "this"), first_connector,                              "Reconnect failure: Closed connection is the first connector");
    ok((ev = test_tap_ev_shift_wait(2)) != NULL,                             "Reconnect failure: Got a server event");
    is_eq(tap_ev_identifier(ev), "test_event_connected",                     "Reconnect failure: It's a connected event");
    connectee = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));
    ok(connectee != first_connector,                                         "Reconnect failure: It's not the first connector");
    ok((ev = test_tap_ev_shift_wait(2)) != NULL,                             "Reconnect failure: Got another server event");
    is_eq(tap_ev_identifier(ev), "test_event_close",                         "Reconnect failure: It's a closed event");
    is(tap_ev_arg(ev, "this"), connectee,                                    "Reconnect failure: It's not the connectee");
    MOCK_SET_HOOK(getsockopt, getsockopt);

    /* Test send failures. */

    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_ERROR_NO_CONNECTION,
                                                                             "Send failure: Can't write to connector when it is not connected");

    /* Reconnect:
     * - ubuntu events: connect, read/close, accept
     * - redhat events: accept , read/close, connect
     * - win32  events: read/close, connect, accept *sometimes*
     */
    first_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_client_connected, test_event_client_read, test_event_client_close);
    is(sxe_connect(first_connector, "127.0.0.1", SXE_LOCAL_PORT(listener)), SXE_RETURN_OK,  "Send failure: Reconnecting connector");
    ok((ev = test_tap_ev_queue_shift_wait(client_queue, 2)) != NULL,         "Send failure: Got client event");
    is_eq(tap_ev_identifier(ev), "test_event_client_connected",              "Send failure: Client connected event");
    is(tap_ev_arg(ev, "this"), first_connector,                              "Send failure: It's on the first connector");
    ok((ev = test_tap_ev_shift_wait(2)) != NULL,                             "Send failure: Got a server event");
    is_eq(tap_ev_identifier(ev), "test_event_connected",                     "Send failure: Server connected event (connectee)");
    connectee = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));
    ok(connectee != first_connector,                                         "Send failure: It's not on the first connector");

    /* Fake a stale read on the connectee (for code coverage)
     */
    connectee->socket = -1;
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_OK, "Fake stale read: Write succeeded");

    MOCK_SET_HOOK(send, test_mock_send);
    test_mock_send_error = SXE_SOCKET_ERROR(ECONNRESET);
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_ERROR_NO_CONNECTION,
                                                                             "Fake stale read: Can't write to connectee when connection reset");

    ok((ev = test_tap_ev_shift_wait(2))   != NULL,                           "Fake stale read: Got another event");
    is_eq(tap_ev_identifier(ev), "test_mock_send",                           "Fake stale read: It's a send event");
    is(tap_ev_arg(ev, "sock"),   first_connector->socket,                    "Fake stale read: First connection sent");
    tap_ev_free(ev);

    test_mock_send_error = SXE_SOCKET_ERROR(EWOULDBLOCK);
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_WARN_WOULD_BLOCK,
                                                                             "Fake stale read: Can't write to connectee when it would block");
    ok((ev = test_tap_ev_shift_wait(2))   != NULL,                           "Fake stale read: Got another event");
    is_eq(tap_ev_identifier(ev), "test_mock_send",                           "Fake stale read: It's a send event");
    is(tap_ev_arg(ev, "sock"),   first_connector->socket,                    "Fake stale read: First connection sent");
    tap_ev_free(ev);
    MOCK_SET_HOOK(send, send);

    /* Double close the listener
     */
    sxe_close(listener);
    sxe_close(listener);
    test_process_all_libev_events();    /* Post process any unexpected events triggered by the close. */
    is(tap_ev_length(), 0,                                                   "Double close: No further events generated");

    is(sxe_fini(), SXE_RETURN_OK,                                            "finished with sxe");
    return exit_status();
}
