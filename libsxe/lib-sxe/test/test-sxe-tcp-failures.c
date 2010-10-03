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
#include "test/common.h"

#define TEST_PORT 9191

SXE * listener;
SXE * connectee = NULL;
SXE * first_connector;
SXE * second_connector;

static void
test_event_connected(SXE * this)
{
    SXEE60I("test_event_connected()");
    SXEL63I("tap_ev_push(%s=%d, this=%p);", __func__, 1, this);
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE62I("test_event_read(this->socket=%d, length=%d)", this->socket, length);
    SXEL65I("tap_ev_push(%s=%d, this=%p, length=%d, buf_used=%d);", __func__, 3, this, length, SXE_BUF_USED(this));
    tap_ev_push(__func__, 3, "this", this, "length", length, "buf_used", SXE_BUF_USED(this));
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE61I("test_event_close(this->socket=%d)", this->socket);
    SXEL64I("tap_ev_push(%s=%d, this=%p, buf_used=%d);", __func__, 2, this, SXE_BUF_USED(this));
    tap_ev_push(__func__, 2, "this", this, "buf_used", SXE_BUF_USED(this));
    SXER60I("return");
}

static void *
test_mock_calloc(size_t nmemb, size_t size)
{
    SXEE62("test_mock_calloc(nmemb=%u, size=%u)", nmemb, size);
    SXE_UNUSED_PARAMETER(nmemb);
    SXE_UNUSED_PARAMETER(size);
    SXER60("return NULL");
    return NULL;
}

static int SXE_STDCALL
test_mock_accept(SXE_SOCKET sock, struct sockaddr * peer_addr, SXE_SOCKLEN_T * peer_addr_size)
{
    SXEE63("test_mock_accept(sock=%d,peer_addr=%p,peer_addr_size=%p)", sock, peer_addr, peer_addr_size);
    SXEL65("tap_ev_push(%s=%d, sock=%p, peer_addr=%p, peer_addr_size=%d);", __func__, 3, sock, tap_dup(peer_addr, *peer_addr_size), *peer_addr_size);
    tap_ev_push(__func__, 3, "sock", sock, "peer_addr", tap_dup(peer_addr, *peer_addr_size), "peer_addr_size", *peer_addr_size);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EINVAL));
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
    SXE_UNUSED_PARAMETER(sock);
    SXE_UNUSED_PARAMETER(addr);
    SXE_UNUSED_PARAMETER(len);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EFAULT));
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

static ssize_t SXE_STDCALL
test_mock_send(SXE_SOCKET sock, const void *buf, SXE_SOCKET_SSIZE count, int flags)
{
    SXEE64("test_mock_send(sock=%d, buf=%p, count=%d, flags=%d)", sock, buf, count, flags);
    SXEL66("tap_ev_push(%s=%d, sock=%p, buf=%p, count=%d, flags=%d);", __func__, 4, sock, tap_dup(buf, count), count, flags);
    tap_ev_push(__func__, 4, "sock", sock, "buf", tap_dup(buf, count), "count", count, "flags", flags);
    sxe_socket_set_last_error(test_mock_send_error);
    return -1;
}

int
main(void)
{
    SXE_BOOL first_connect_confirmed = SXE_FALSE;
    tap_ev   event1;
    tap_ev   event2;
    tap_ev   event3;
    tap_ev   ev;

    plan_tests(70);

     /* Initialization failure cases.
     */
    sxe_register(1, 0); /* first plugin registers */

    MOCK_SKIP_START(1);
    MOCK_SET_HOOK(calloc, test_mock_calloc);
    ok(sxe_init() == SXE_RETURN_ERROR_ALLOC,                         "sxe_init(): Init failed as expected");
    MOCK_SET_HOOK(calloc, calloc);
    MOCK_SKIP_END;

    ok(sxe_fini() == SXE_RETURN_ERROR_INTERNAL,                      "sxe_fini(): Fini failed as expected - won't work unless init was successful");

    /* Successfully initialize for remaining tests. Add another 2 objects for a total of 3:
     *  - 1 listener
     *  - 2 connectors
     *  - 1 connectee
     */
    sxe_register(3, 4);
    is(sxe_init(),                  SXE_RETURN_OK,                   "sxe_init(): Initialization succeeded");

    /* Create a listener and test listen failure cases.
     */
    listener = sxe_new_tcp(NULL, "INADDR_ANY", TEST_PORT, test_event_connected, test_event_read, test_event_close);

    MOCK_SET_HOOK(socket, test_mock_socket);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,       "sxe_listen(): Can't listen on SXE object when socket() fails");
    MOCK_SET_HOOK(socket, socket);

    MOCK_SET_HOOK(bind, test_mock_bind_eaddrinuse);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_ADDRESS_IN_USE, "sxe_listen(): Can't listen on SXE object when bind() fails");
    MOCK_SET_HOOK(bind, bind);
    is(sxe_socket_get_last_error(), SXE_SOCKET_ERROR(EADDRINUSE),    "sxe_listen(): As expected, bind failed due to address already being in use");

    MOCK_SET_HOOK(listen, test_mock_listen);
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,       "sxe_listen(): Can't listen on SXE object when listen() fails");
    MOCK_SET_HOOK(listen, listen);

    is(sxe_listen(listener),        SXE_RETURN_OK,                   "sxe_listen(): Creation of listener succeeded");
    is(sxe_listen(listener),        SXE_RETURN_ERROR_INTERNAL,       "sxe_listen(): Double listen on listener fails");

    /* Accept failure cases. Allocate first_connector, and test.
     */

    ok((first_connector = sxe_new_tcp(NULL, "127.0.0.1", 0, test_event_connected, test_event_read, test_event_close)) != NULL,
                                                                     "1st connector: Allocated first connector");

    MOCK_SET_HOOK(accept, test_mock_accept);
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_OK, "1st connector: Initiated connection on first connector");

    ok((event1 = test_tap_ev_shift_wait(2)),                                 "1st connector: Got first  event // e.g. accept() system call");
    ok((event2 = test_tap_ev_shift_wait(2)),                                 "1st connector: Got second event // e.g. sxe event connected" );

    if (strcmp(tap_ev_identifier(event1), "test_event_connected") == 0) {
        tap_ev swap = event1;
        event1      = event2;
        event2      = swap;
    }

    is_eq(tap_ev_identifier(event1), "test_mock_accept",                     "1st connector: First  event is accept() system call");
    is_eq(tap_ev_identifier(event2), "test_event_connected",                 "1st connector: Second event is sxe event connected" );

    MOCK_SET_HOOK(accept, accept);
    ok((ev = test_tap_ev_shift_wait(2)) != NULL,                             "1st connector: Got another event"         );
    is_eq(tap_ev_identifier(ev),     "test_event_connected",                 "1st connector: Third  event is sxe event connected");

    if (tap_ev_arg(ev, "this") == first_connector) {
        if (first_connect_confirmed) {
            fail("Extra zero length read on first connector");
        }
        else {
            tap_ev_free(ev);
            ok((ev = tap_ev_shift()) != NULL,          "1st connector: Got another event"        );
            is(tap_ev_identifier(ev), test_event_read, "1st connector: Event is sxe event read"  );
            is(tap_ev_arg(ev, "length"), 0,            "1st connector: 0 bytes due to connect"   );
            is(tap_ev_arg(ev, "buf_used"), 0,          "1st connector: 0 bytes in receive buffer");
            first_connect_confirmed = SXE_TRUE;
        }
    }

    tap_ev_free(ev);
    is(tap_ev_length(), 0, "1st connector: No more events for now");

    ok((second_connector = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close)) != NULL, "2nd connector: Allocated second connector");
    is(sxe_connect(second_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_OK, "2nd connector: Initiated connection on second connector");

    ok((event1 = test_tap_ev_shift_wait(2)),                 "2nd connector: Got first  event // e.g. sxe event connected");
    ok((event2 = test_tap_ev_shift_wait(2)),                 "2nd connector: Got second event // e.g. sxe event close" );

    if (strcmp(tap_ev_identifier(event1), "test_event_close") == 0) {
        tap_ev swap = event1;
        event1      = event2;
        event2      = swap;
    }

    is_eq(tap_ev_identifier(event1), "test_event_connected", "2nd connector: First  event is sxe event connected");
    is_eq(tap_ev_identifier(event2), "test_event_close",     "2nd connector: Second event is sxe event close"    );
    is(tap_ev_arg(event2, "this"), second_connector,         "2nd connector: Second connector closed"            );
    is(tap_ev_arg(event2, "buf_used"), 0,                    "2nd connector: 0 bytes in receive buffer"          );

    /* Close and reallocate connection.
     */
    sxe_close(first_connector);
    ok((first_connector = sxe_new_tcp(NULL, "127.0.0.1", 0, test_event_connected, test_event_read, test_event_close)) != NULL,
                                                     "Reallocate: Reallocated first connector");
    ok((ev = test_tap_ev_shift_wait(2))   != NULL,   "Reallocate: Got another event"                );
    is_eq(tap_ev_identifier(ev), "test_event_close", "Reallocate: It's another close event"         );
    ok(tap_ev_arg(ev, "this") !=  first_connector,   "Reallocate: First connection close indication");
    is(tap_ev_arg(ev, "buf_used"), 0,                "Reallocate: 0 bytes in receive buffer"        );

    /* Test connection failures.
     */
    MOCK_SET_HOOK(socket, test_mock_socket);
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_ERROR_INTERNAL,
                                                   "Connection failure: Can't connect to listener when socket() fails");
    MOCK_SET_HOOK(socket, socket);

    MOCK_SET_HOOK(bind, test_mock_bind_efault);
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_ERROR_INTERNAL,
                                                   "Connection failure: Can't connect to listener when bind() fails");
    MOCK_SET_HOOK(bind, bind);

    MOCK_SET_HOOK(bind, test_mock_bind_eaddrinuse);
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_ERROR_ADDRESS_IN_USE,
                                                   "Connection failure: Can't connect to listener when address is in use");
    MOCK_SET_HOOK(bind, bind);


    MOCK_SET_HOOK(connect, test_mock_connect);
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_ERROR_INTERNAL,
                                                   "Connection failure: Can't connect to listener when connect() fails");
    MOCK_SET_HOOK(connect, connect);
    sxe_close(listener);

    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_OK,
                                                    "Connection failure: Reconnecting to listener when listener is closed");

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_ONESHOT);

    ok((ev = tap_ev_shift())   != NULL,              "Connection failure: Got another event");
    is_eq(tap_ev_identifier(ev), "test_event_close", "Connection failure: It's another close event");
    is(tap_ev_arg(ev, "this"), first_connector,      "Connection failure: First connection close indication" );
    is(tap_ev_arg(ev, "buf_used"), 0,                "Connection failure: 0 bytes in receive buffer");

    is(sxe_listen(listener), SXE_RETURN_OK,           "Reconnect failure: Recreated listener");
    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_OK,
                                                      "Reconnect failure: Reconnecting to listener to test getsockopt failure");
    MOCK_SET_HOOK(getsockopt, test_mock_getsockopt);

    ok((event1 = test_tap_ev_shift_wait(2)),          "Reconnect failure: Got First Event");
    ok((event2 = test_tap_ev_shift_wait(2)),          "Reconnect failure: Got Second Event");

    if (strcmp(tap_ev_identifier(event1), "test_event_close") == 0) {
        tap_ev swap = event1;
        event1      = event2;
        event2      = swap;
    }

    is_eq(tap_ev_identifier(event1), "test_event_connected",  "Reconnect failure: Got connected event");
    ok(tap_ev_arg(event1, "this") != first_connector,         "Reconnect failure: Accepted connection is not the first connector");

    is_eq(tap_ev_identifier(event2), "test_event_close",      "Reconnect failure: Got Close event");
    is(tap_ev_arg(event2, "this"), first_connector,           "Reconnect failure: It's the first connector");
    is(tap_ev_arg(event2, "buf_used"), 0,                     "Reconnect failure: 0 bytes in receive buffer");

    MOCK_SET_HOOK(getsockopt, getsockopt);

    /* Test send failures.
     */
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_ERROR_NO_CONNECTION,
                                                                            "Send failure: Can't write to connector when it is not connected");

    is(sxe_connect(first_connector, "127.0.0.1", TEST_PORT), SXE_RETURN_OK, "Send failure: Reconnecting connector");

    ok((event1 = test_tap_ev_shift_wait(2)),          "Send failure: Got First  Event");
    ok((event2 = test_tap_ev_shift_wait(2)),          "Send failure: Got Second Event");
    ok((event3 = test_tap_ev_shift_wait(2)),          "Send failure: Got Third  Event");

    /* ubuntu events: connect, read/close, accept  */
    /* redhat events: accept , read/close, connect */
    /* win32  events: read/close, connect, accept   *sometimes* */
    /* Re-order events to match ubuntu by looking for the connect on the first_connector*/
    if ((strcmp(tap_ev_identifier(event3), "test_event_connected") == 0)
    &&  (       tap_ev_arg       (event3, "this") == first_connector   )) {
        tap_ev swap = event1;
        event1      = event3;
        event3      = swap;
    }
    else if ((strcmp(tap_ev_identifier(event2), "test_event_connected") == 0)
         &&  (       tap_ev_arg       (event2, "this") == first_connector   )) {
        tap_ev swap = event1;
        event1      = event2;
        event2      = swap;
    }

    is_eq(tap_ev_identifier(event1), "test_event_connected",  "Send failure: 1st connected event");
    is(tap_ev_arg(event1, "this"), first_connector,           "Send failure: It's on the first connector");

    is_eq(tap_ev_identifier(event2), "test_event_close",      "Send failure: Close event (sxe=%u)",
          SXE_ID((const SXE *)tap_ev_arg(ev, "this")));

    is_eq(tap_ev_identifier(event3), "test_event_connected",  "Send failure: 2nd connected event (connectee)");
    connectee = (SXE *)(unsigned long)tap_ev_arg(event3, "this");
    ok(connectee != first_connector,                          "Send failure: It's not on the first connector");

    /* Fake a stale read on the connectee (for code coverage)
     */
    connectee->socket = -1;
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_OK, "Fake stale read: Write succeeded");

    MOCK_SET_HOOK(send, test_mock_send);
    test_mock_send_error = SXE_SOCKET_ERROR(ECONNRESET);

    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_ERROR_NO_CONNECTION,
                                                              "Fake stale read: Can't write to connectee when connection reset");

    ok((ev = test_tap_ev_shift_wait(2))   != NULL,            "Fake stale read: Got another event");
    is_eq(tap_ev_identifier(ev), "test_mock_send",            "Fake stale read: It's a send event");
    is(tap_ev_arg(ev, "sock"),   first_connector->socket,     "Fake stale read: First connection sent");
    tap_ev_free(ev);

    test_mock_send_error = SXE_SOCKET_ERROR(EWOULDBLOCK);
    is(sxe_write(first_connector, "should not get this", 20), SXE_RETURN_WARN_WOULD_BLOCK,
                                                              "Fake stale read: Can't write to connectee when it would block");
    ok((ev = test_tap_ev_shift_wait(2))   != NULL,            "Fake stale read: Got another event");
    is_eq(tap_ev_identifier(ev), "test_mock_send",            "Fake stale read: It's a send event");
    is(tap_ev_arg(ev, "sock"),   first_connector->socket,     "Fake stale read: First connection sent");
    tap_ev_free(ev);
    MOCK_SET_HOOK(send, send);

    /* Double close the listener
     */
    sxe_close(listener);
    sxe_close(listener);
    ev_loop_nonblock();    /* Post process any unexpected events triggered by the close. */
    is(tap_ev_length(), 0, "Double close: No further events generated");

    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");
    return exit_status();
}
