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

#include <assert.h>
#include <string.h>
#include <sys/types.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "sxe-pool-tcp.h"
#include "sxe-pool-tcp-private.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "tap.h"

#ifdef WINDOWS_NT
#else

#define NO_TIMEOUT 0.0
#define WAIT       2.0

static tap_ev_queue   server_queue                 = NULL;

static inline tap_ev test_next_client_event(void) { return test_tap_ev_shift_wait(                    WAIT); }
static inline tap_ev test_next_server_event(void) { return test_tap_ev_queue_shift_wait(server_queue, WAIT); }

static void
test_event_timeout(SXE_POOL_TCP * pool, void * info)
{
    SXEE63("%s(pool=%s, info=%p)", __func__, SXE_POOL_TCP_GET_NAME(pool), info);
    SXE_UNUSED_ARGUMENT(pool);
    SXE_UNUSED_ARGUMENT(info);
    SXEA10(0, "test_event_timeout should never be called in this test");
    SXER60("return");
}

static void
test_event_client_write(SXE_POOL_TCP * pool, void * info)
{
    SXEE82("test_event_client_write(pool=%s, info=%p)", SXE_POOL_TCP_GET_NAME(pool), info);
    tap_ev_push(__func__, 1, "pool", pool, "info", info);
    SXER80("return");
}

static void
test_event_client_connect(SXE * this)
{
    SXEE81I("%s()", __func__);
    tap_ev_push(__func__, 1, "this", this);
    SXER80I("return");
}

static void
test_event_client_read(SXE * this, int length)
{
    SXEE81I("test_event_client_read(length=%d)", length);
    SXEA10I(length != 0, "test_event_client_read called with length == 0");
    tap_ev_push(__func__, 4, "this", this, "user_data", SXE_USER_DATA(this), "length", length, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)));
    SXE_BUF_CLEAR(this);
    SXER80I("return");
}

static void
test_event_client_close(SXE * this)
{
    SXEE80I("test_event_client_close()");
    SXE_UNUSED_ARGUMENT(this);
    tap_ev_push(__func__, 1, "this", this);
    SXER80I("return");
}

/* TODO: Send data on connection indication and make sure we handle it gracefully. */

static void
test_event_server_connect(SXE * this)
{
    SXEE81I("test_event_server_connect(this=%p)", this);
    tap_ev_queue_push(server_queue, __func__, 2, "this", this);
    SXER80I("return");
}

static void
test_event_server_read(SXE * this, int length)
{
    SXEE81I("test_event_server_read(length=%d)", length);
    tap_ev_queue_push(server_queue, __func__, 3, "this", this, "length", length, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)));
    SXE_BUF_CLEAR(this);
    SXER80I("return");
}

static void
test_event_server_close(SXE * this)
{
    SXEE80I("test_event_server_close()");
    tap_ev_queue_push(server_queue, __func__, 1, "this", this);
    SXER80I("return");
}

static void
test_case_setup(void)
{
}

static void
test_case_cleanup(void)
{
    tap_ev ev;

    test_process_all_libev_events();               /* Don't block, but queue any events that happen to be ready */
    SXEA11((ev = tap_ev_shift())                   == NULL, "No unprocessed events in the client queue (%s)", tap_ev_identifier(ev));
    SXEA11((ev = tap_ev_queue_shift(server_queue)) == NULL, "No unprocessed events in the server queue (%s)", tap_ev_identifier(ev));
    sxe_fini();
}

static SXE *
test_expect_server_read(const char * expected_buf, unsigned expected_length)
{
    tap_ev ev;
    SXE *  this;

    SXEE63("%s(expected_buf=%p, expected_length=%d)", __func__, expected_buf, expected_length);

    ok((ev = test_next_server_event()) != NULL,                      "Event occurred");
    is_eq(tap_ev_identifier(ev), "test_event_server_read",           "Got a server read event");
    is(SXE_CAST(int, tap_ev_arg(ev, "length")), expected_length,     "Server received %u characters", expected_length);
    this = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));

    /* If the expected buffer contains a string that is shorter than the expected length, just make sure the received buffer
     * starts with the string.
     */
    if (strlen(expected_buf) < expected_length) {
        is_strncmp(tap_ev_arg(ev, "buf"), expected_buf, strlen(expected_buf), "Server received message matched /%s%.*s/",
                   expected_buf, (int)(expected_length - strlen(expected_buf)), "................");
    }
    else {
        is_strncmp(tap_ev_arg(ev, "buf"), expected_buf, strlen(expected_buf), "Server received '%s'", expected_buf);
    }

    SXER61("return // this=%p", this);
    return this;
}

static void
test_check_ready_to_write(SXE_POOL_TCP * pool, unsigned expected, const char * func, unsigned line)
{
    unsigned i;

    /* Wait for and count any ready_to_write events we expect */
    for (i = 0; i < expected; i++) {
        sxe_pool_tcp_queue_ready_to_write_event(pool);

        if (test_tap_ev_shift_wait(WAIT) == NULL) {
            break;
        }
    }

    is(i, expected, "%s:%u:%u ready to write events have occurred", func, line, expected);
}

static void
test_case_happy_path(void)
{
    SXE_POOL_TCP * pool;
    SXE *          listener;
    char           buf[1024];
    SXE *          this;
    unsigned short test_port;
    tap_ev         ev;
    unsigned       i;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read, test_event_server_close);
    ok(listener != 0,                       "Created a listener");
    is(sxe_listen(listener), SXE_RETURN_OK, "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    pool = sxe_pool_tcp_new_connect(2, "happy_pool", "127.0.0.1", test_port, test_event_client_write,
                                    test_event_client_connect, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool != NULL, "happy_pool was initialized");
    is_eq(tap_ev_identifier(test_next_client_event()), "test_event_client_connect", "1st client connect");
    is_eq(tap_ev_identifier(test_next_client_event()), "test_event_client_connect", "2nd client connect");

    /* Now show that we can write stuff to the connection */

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello", 5, buf), SXE_RETURN_OK, "Successfully wrote 'Hello' to the tcp pool object");
    this = test_expect_server_read("Hello", 5);
    sxe_write(this, "There", 5);
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_read", "1st client read");
    is(SXE_CAST(int, tap_ev_arg(ev, "length")), 5,          "Five bytes of data received on the tcp pool object");
    is_strncmp(tap_ev_arg(ev, "buf"), "There", 5,           "Received 'There' on the tcp pool object");

    /* Two sends at a time */

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello2", 6, buf), SXE_RETURN_OK,    "Successfully wrote 'Hello2' to the tcp pool object");
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello3", 6, buf), SXE_RETURN_OK,    "Successfully wrote 'Hello3' to the tcp pool object");

    /* Two responses at a time */

    this = test_expect_server_read("Hello", 6);
    sxe_write(this, "xxx", 3);
    this = test_expect_server_read("Hello", 6);
    sxe_write(this, "xxx", 3);

    for (i = 0; i < 2; i++) {
        is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_read", "2nd or 3rd client read");
        is(SXE_CAST(int, tap_ev_arg(ev, "length")), 3,          "3 bytes of data received on the tcp pool object");
        is_strncmp(tap_ev_arg(ev, "buf"), "xxx", 3,             "Received 'xxx' on the tcp pool object");
    }

    /* Shut 'er down */

    sxe_pool_tcp_delete(NULL, pool);
    is_eq(tap_ev_identifier(test_next_server_event()), "test_event_server_close", "1st server connection closed");
    is_eq(tap_ev_identifier(test_next_server_event()), "test_event_server_close", "2nd server connection closed");

    sxe_close(listener);
    test_case_cleanup();
}

static void
test_case_connection_closed_and_reopened(void)
{
    SXE_POOL_TCP   * pool;
    SXE            * listener;
    SXE            * this;
    unsigned short   test_port;
    tap_ev           ev;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_server_connect, test_event_server_read, test_event_server_close);

    ok(listener                                          != NULL,                        "Created a listener");
    is(sxe_listen(listener),                                SXE_RETURN_OK,               "Listened on the listener");
    test_port = SXE_LOCAL_PORT(listener);

    pool = sxe_pool_tcp_new_connect(2, "recon_pool", "127.0.0.1", test_port, test_event_client_write,
                                    test_event_client_connect, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool != NULL, "recon_pool was initialized");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_connect", "1st client connect");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_connect", "2nd client connect");
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_connect", "1st server connect");
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_connect", "2nd server connect");

    test_check_ready_to_write(pool, 2, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "close_one_", 10, (void *)0xDEADBABE), SXE_RETURN_OK,    "Successfully wrote 'close_one_' to the tcp pool object");
    this = test_expect_server_read("close_one_", 10);
    sxe_write(this, "_one_", 5);
    sxe_close(this);

    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_read",    "Received the response");
    is_strncmp(tap_ev_arg(ev, "buf"),                       "_one_", 5,                  "Response data is as expected");
    is(tap_ev_arg(ev, "user_data"),                         0xDEADBABE,                  "User data is 0xDEADBABE as expected (%p)",
       tap_ev_arg(ev, "user_data"));

    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_close",   "Client got the close indication");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_connect", "Connection re-opened (client indication)");
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_connect", "Connection re-opened (server connect)");

    sxe_pool_tcp_delete(NULL, pool);
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_close",   "first server connection was closed");
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_close",   "second server connection was closed");

    sxe_close(listener);
    test_case_cleanup();
}

/*
 * If someone overallocates their registered quota, the pool can be starved. Simulate this. In future, get rid of this
 *
 * @note Must use sockets to implement the server to prevent it from interfering with the SXE TCP pool
 */
static void
test_case_pool_too_big_to_service(void)
{
    SXE_SOCKET         listener;
    struct sockaddr_in address;
    SXE_SOCKLEN_T      addr_len;
    unsigned short     test_port;
    SXE_POOL_TCP     * big_pool;
    tap_ev             ev;

    test_case_setup();

    /* Start the server */

    SXEA11((listener = socket(AF_INET, SOCK_STREAM, 0)) != SXE_SOCKET_INVALID, "Failed to allocate server listener socket: %s",
           sxe_socket_get_last_error_as_str());

    address.sin_family      = AF_INET;
    address.sin_port        = 0;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    SXEA11(bind(listener, (struct sockaddr *)&address, sizeof(address)) >= 0,  "Failed to bind server listener socket: %s",
           sxe_socket_get_last_error_as_str());

    addr_len = sizeof(address);
    SXEA11(getsockname(listener, (struct sockaddr *)&address, &addr_len) >= 0, "Failed to get server listener address: %s",
           sxe_socket_get_last_error_as_str());
    test_port = ntohs(address.sin_port);

    SXEA11(listen(listener, SOMAXCONN) >= 0,                                   "Failed to listen on server listener socket: %s",
           sxe_socket_get_last_error_as_str());

    sxe_pool_tcp_register(5);
    is(sxe_init(),                                          SXE_RETURN_OK,               "Initialized SXE");

    big_pool = sxe_pool_tcp_new_connect(20, "bigpool", "127.0.0.1", test_port, test_event_client_write,
                                        test_event_client_connect, test_event_client_read, test_event_client_close,
                                        NO_TIMEOUT, NO_TIMEOUT, test_event_timeout, NULL);
    ok(big_pool                                          != NULL,                        "Created big pool of 20 connections");
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_connect", "1st client connected");
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_connect", "2nd client connected");
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_connect", "3rd client connected");
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_connect", "4th client connected");
    is_eq(tap_ev_identifier(ev = test_next_client_event()), "test_event_client_connect", "5th client connected");

    /* Delete the pool, but don't test for close events - we don't care, will never do this in production.
     */
    test_process_all_libev_events();        /* Don't block, but queue any events that happen to be ready */
    sxe_pool_tcp_delete(NULL, big_pool);
    CLOSESOCKET(listener);
    test_case_cleanup();
}

static void
test_case_max_capacity(void)
{
    SXE_POOL_TCP * pool;
    SXE          * listener;
    char           buf[1024];
    SXE          * sxe_first;
    SXE          * sxe_second;
    unsigned short test_port;
    tap_ev         ev;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(),                                          SXE_RETURN_OK,               "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read, test_event_server_close);
    ok(listener                                          != NULL,                        "Created a listener");
    is(sxe_listen(listener),                                SXE_RETURN_OK,               "Listening on a server TCP port");
    test_port = SXE_LOCAL_PORT(listener);

    pool = sxe_pool_tcp_new_connect(2, "max_capacity_pool", "127.0.0.1", test_port, test_event_client_write,
                                    test_event_client_connect, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool                                              != NULL,                        "max_capacity_pool was initialized");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_connect", "1st client connected");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_connect", "2nd client connected");

    /* Write to the connections we've established */

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "max one", 7, buf),         SXE_RETURN_OK,               "Wrote 'max one'");
    is_eq(tap_ev_identifier(ev = test_next_server_event()), "test_event_server_read",    "1st request read by server");
    sxe_first = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "max two", 7, buf),         SXE_RETURN_OK,               "Wrote 'max two'");
    is_eq(tap_ev_identifier(ev = test_next_server_event()), "test_event_server_read",    "2nd request read by server");
    sxe_second = SXE_CAST_NOCONST(SXE *, tap_ev_arg(ev, "this"));

    /* Make sure that the connections are different */

    ok(sxe_first != sxe_second,                                                          "Server reads are on different SXEs");
    sxe_pool_tcp_queue_ready_to_write_event(pool);    /* Should not trigger an event yet                           */
    test_process_all_libev_events();                  /* Don't block, but queue any events that happen to be ready */
    is(tap_ev_length(), 0,                                                               "No events are queued (blocked)");

    is(sxe_write(sxe_first, "xxx", 3),                      SXE_RETURN_OK,               "Respond to the first request");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_read",    "Client got the response");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_write",   "Client is now ready to write");

    is(sxe_write(sxe_second, "xxx", 3),                     SXE_RETURN_OK,               "Respond to the second request");
    is_eq(tap_ev_identifier(test_next_client_event()),      "test_event_client_read",     "Client got the response");
    /* Shouldn't get another ready to write event, because we didn't ask for one with sxe_pool_tcp_queue_ready_to_write_event */

    /* Intentionally closing the server listener down first here
     */
    sxe_close(listener);
    sxe_pool_tcp_delete(NULL, pool);
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_close",   "first server connection was closed");
    is_eq(tap_ev_identifier(test_next_server_event()),      "test_event_server_close",   "second server connection was closed");
    test_case_cleanup();
}

#endif

int
main(void)
{
#ifdef WINDOWS_NT
    diag("TODO: sxe-pool-tcp test cases are disabled on Windows (probable lib-ev on Windows bug)\n");
    return 0;
#else
    plan_tests(85);

    server_queue = tap_ev_queue_new();

    test_case_happy_path();
    test_case_connection_closed_and_reopened();
    test_case_pool_too_big_to_service();
    test_case_max_capacity();
    return exit_status();
#endif
}

