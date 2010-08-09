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

#define NO_TIMEOUT 0.0

static unsigned int   connect_count                = 0;
static unsigned int   connected_count              = 0;
static unsigned int   close_count                  = 0;
static unsigned int   ready_to_write_count         = 0;
static SXE          * listen_read_last_sxe         = NULL;
static char           listen_read_buffer[1024];
static unsigned int   read_length_sum              = 0;
static unsigned int   listen_read_length_sum       = 0;
static void         * caller_info                  = NULL;

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
test_event_client_ready_to_write(SXE_POOL_TCP * pool, void * info)
{
    SXEE82("test_event_client_ready_to_write(pool=%s, info=%p)", SXE_POOL_TCP_GET_NAME(pool), info);
    SXE_UNUSED_ARGUMENT(pool);
    SXE_UNUSED_ARGUMENT(info);
    ++ready_to_write_count;
    SXER80("return");
}

static void
test_event_client_connected(SXE* this)
{
    SXEE80I("test_event_client_connected()");
    SXE_UNUSED_ARGUMENT(this);
    tap_ev_push(__func__, 1, "this", this);
    SXER80I("return");
}

static void
test_event_client_read(SXE* this, int length)
{
    void * user_data;

    SXEE81I("test_event_client_read(length=%d)", length);
    SXEA10I(length != 0, "test_event_client_read called with length == 0");
    tap_ev_push(__func__, 2, "this", this, "length", length);
    read_length_sum += length;

    if ((user_data = SXE_USER_DATA(this)) != NULL)
    {
        SXEL80I("Copying read event contents into user data");
        memcpy(user_data, SXE_BUF(this), length);
        ((char*)(user_data))[length] = '\0';
    }

    SXE_BUF_CLEAR(this);
    SXER80I("return");
}

static void
test_event_client_close(SXE* this)
{
    SXEE80I("test_event_client_close()");
    SXE_UNUSED_ARGUMENT(this);
    tap_ev_push(__func__, 1, "this", this);
    ++close_count;
    SXER80I("return");
    return;
}

/* TODO: Send data on connection indication and make sure we handle it gracefully. */

static void
test_event_server_read_and_send(SXE* this, int length)
{
    SXEE81I("test_event_server_read_and_send(length=%d)", length);
    tap_ev_push(__func__, 2, "this", this, "length", length);
    SXER80I("return");
}

static void
test_event_server_read_and_keep_state(SXE* this, int length)
{
    SXEE81I("test_event_server_read_and_keep_state(length=%d)", length);
    tap_ev_push(__func__, 1, "this", this);
    listen_read_length_sum += length;
    listen_read_last_sxe = this;
    strncpy(listen_read_buffer, SXE_BUF(this), length);
    SXER80I("return");
}

static void
test_event_server_close(SXE* this)
{
    SXEE80I("test_event_server_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER80I("return");
}

static int
test_consume_next_named_event(const char * expected_event_name)
{
    tap_ev event = NULL;
    int result = 0;

    SXEE62("%s(expected_event_name=%s)", __func__, expected_event_name);

    event = test_tap_ev_shift_wait(2);
    if (event == NULL) {
        diag("Expecting an event, but didn't get one.");
        goto SXE_ERROR_OUT;
    }
    if (strcmp(tap_ev_identifier(event), expected_event_name) != 0) {
        diag("Expecting event '%s'\n#       Got event '%s'", expected_event_name, (const char *)(tap_ev_identifier(event)));
        goto SXE_ERROR_OUT;
    }

    result = 1;

SXE_ERROR_OUT:
    if (event != NULL) {
        tap_ev_free(event);
    }
    SXER61("return %d", result);
    return result;
}

static int SXE_STDCALL
test_mock_connect(SXE_SOCKET sockfd, const struct sockaddr *serv_addr, SXE_SOCKLEN_T addrlen)
{
    int result = 0;

    SXEE83("test_mock_connect(sockfd=%d, serv_addr=%p, addrlen=%d)", sockfd, serv_addr, addrlen);
    SXE_UNUSED_ARGUMENT(sockfd);
    SXE_UNUSED_ARGUMENT(serv_addr);
    SXE_UNUSED_ARGUMENT(addrlen);
    ++connect_count;
    MOCK_SET_HOOK(connect, connect);
    result = connect(sockfd, serv_addr, addrlen);
    MOCK_SET_HOOK(connect, test_mock_connect);
    SXER81("return %d", result);
    return result;
}

static void
process_all_libev_events(unsigned number_of_expected_events)
{
    SXEE61("process_all_libev_events(number_of_expected_events=%d)", number_of_expected_events);

    /* TODO: work out if there's a better way of processessing all outstanding
     * libev events, including new events which are generated by our callbacks
     * */
    while (number_of_expected_events) {
        test_ev_loop_wait(2);
        --number_of_expected_events;
    }

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    SXER60("return");
    return;
}


static void
test_case_setup(void)
{
    assert(tap_ev_length() == 0);
    connect_count        = 0;
    connected_count      = 0;
    read_length_sum      = 0;
    ready_to_write_count = 0;
    close_count          = 0;
}

static void
test_case_cleanup(void)
{
    MOCK_SET_HOOK(connect, connect);
    process_all_libev_events(0);
    tap_ev_flush();
}

static void *
test_expect_server_read_and_send(const char * expected_buf, unsigned expected_length)
{
    tap_ev ev;
    SXE *  this;

    SXEE63("%s(expected_buf=%p, expected_length=%d)", __func__, expected_buf, expected_length);

    if (tap_ev_length() == 0) {
        test_ev_loop_wait(2);
    }

    ok((ev = tap_ev_shift()) != NULL,                                 "Event occurred");
    is_eq(tap_ev_identifier(ev), "test_event_server_read_and_send",   "Got a 'server_read_and_send' event");
    is((unsigned)tap_ev_arg(ev, "length"), expected_length,           "Server received %u characters", expected_length);
    assert((this = (SXE *)(long)tap_ev_arg(ev, "this")) != NULL);

    /* If the expected buffer contains a string that is shorter than the expected length, just make sure the received buffer
     * starts with the string.
     */
    if (strlen(expected_buf) < expected_length) {
        is_strncmp(SXE_BUF(this), expected_buf, strlen(expected_buf), "Server received message matched /%s%.*s/",
                   expected_buf, expected_length - strlen(expected_buf), "................");
    }
    else {
        is_strncmp(SXE_BUF(this), expected_buf, strlen(expected_buf), "Server received '%s'", expected_buf);
    }

    SXE_BUF_CLEAR(this);
    tap_ev_free(ev);

    SXER61("return // this=%p", this);
    return this;
}

static void
test_check_ready_to_write(SXE_POOL_TCP * pool, unsigned expected, const char * func, unsigned line)
{
    ready_to_write_count = 0;
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is(ready_to_write_count, expected, "%s:%u:%u ready to write events have occurred", func, line, expected);
}

static void
test_case_happy_path(void)
{
    SXE_POOL_TCP * pool;
    SXE *          listener;
    char           buf[1024];
    SXE *          this;
    unsigned short test_port;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read_and_send, test_event_server_close);
    ok(listener != 0,                       "Created a listener");
    is(sxe_listen(listener), SXE_RETURN_OK, "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    /* NOTE: sxe_pool_tcp_new_connect() will not validate that there's a server
     *       listening at the designated address and port.  No error will be
     *       returned; though attempting to call sxe_pool_tcp_write() will
     *       probably generate an error
     */
    MOCK_SET_HOOK(connect, test_mock_connect);
    pool = sxe_pool_tcp_new_connect(2, "happy_pool", "127.0.0.1", test_port, test_event_client_ready_to_write,
                                    test_event_client_connected, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, caller_info);
    ok(pool != NULL, "happy_pool was initialized");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "1st client connected");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "2nd client connected");
    is(connect_count,        2, "There should be two connects made to the listen socket");
    is(read_length_sum,      0, "read callback was called with zero length for both connections");
    is(ready_to_write_count, 0, "'Ready to Write' event was not called back");

    /*
     * Now show that we can write stuff to the connection
     *
     * Note:
     *  - the SXE connections are pool->node_not_ready_to_send[0..1].sxe
     */

    /* first send */

    read_length_sum   = 0;
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello", 5, buf), SXE_RETURN_OK, "Successfully wrote 'Hello' to the tcp pool object");
    this = test_expect_server_read_and_send("Hello", 5);

    sxe_write(this, "There", 5);
    ok(test_consume_next_named_event("test_event_client_read"), "process response");
    is(read_length_sum, 5,      "Five bytes of data received on the tcp pool object");
    is_strncmp(buf, "There", 5, "Received 'There' on the tcp pool object");

    read_length_sum   = 0;

    /* second send */
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello2", 6, buf), SXE_RETURN_OK,    "Successfully wrote 'Hello2' to the tcp pool object");

    /* third send */
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "Hello3", 6, buf), SXE_RETURN_OK,    "Successfully wrote 'Hello3' to the tcp pool object");

    /* second & third response */
    process_all_libev_events(0);
    is(tap_ev_count("test_event_server_read_and_send"), 2,  "Data written to the tcp pool is received by the listening connection");
    this = test_expect_server_read_and_send("Hello", 6);
    sxe_write(this, "xxx", 3);
    this = test_expect_server_read_and_send("Hello", 6);
    sxe_write(this, "xxx", 3);

    ok(test_consume_next_named_event("test_event_client_read"), "Consume first response");
    ok(test_consume_next_named_event("test_event_client_read"), "Consume second response");

    is(read_length_sum , 6,     "Two responses, each 3 bytes, received on the tcp pool object");
    is_eq(buf,           "xxx", "Received 'xxx' as the second response of the two");

    /* fourth send */
    test_check_ready_to_write(pool, 1, __func__, __LINE__);

    read_length_sum   = 0;
    is(sxe_pool_tcp_write(pool, "Hello", 5, buf), SXE_RETURN_OK, "Successfully wrote 'Hello' to the tcp pool object");

    /* fourth response */
    this = test_expect_server_read_and_send("Hello", 5);

    sxe_write(this, "There", 5);
    test_ev_loop_wait(2);
    is(read_length_sum  , 5,    "Five bytes of data received on the tcp pool object");
    is_strncmp(buf, "There", 3, "Received 'There' as a response");

    sxe_pool_tcp_delete(NULL, pool);
    process_all_libev_events(0);
    sxe_close(listener);
    sxe_fini();

    test_case_cleanup();
}

static void
test_case_connection_closed_and_reopened(void)
{
    SXE_POOL_TCP* pool;
    SXE *         listener;
    char          buf[1024];
    SXE *         this;
    unsigned short test_port;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read_and_send, test_event_server_close);

    ok(listener != NULL,                    "Created a listener");
    is(sxe_listen(listener), SXE_RETURN_OK, "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    MOCK_SET_HOOK(connect, test_mock_connect);
    pool = sxe_pool_tcp_new_connect(2, "recon_pool", "127.0.0.1", test_port, test_event_client_ready_to_write,
                                    test_event_client_connected, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, caller_info);
    ok(pool != NULL, "recon_pool was initialized");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "1st client connected");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "2nd client connected");
    test_check_ready_to_write(pool, 1, __func__, __LINE__);

    connect_count = 0;
    is(close_count, 0,                                                 "Nothing has closed a connection yet");
    is(sxe_pool_tcp_write(pool, "close_one_", 10, buf), SXE_RETURN_OK, "Successfully wrote 'close_one_' to the tcp pool object");
    this = test_expect_server_read_and_send("close_one_", 10);
    sxe_write(this, "_one_", 5);
    sxe_close(this);
    test_ev_loop_wait(2);
    is_strncmp(buf, "_one_", 5, "Received '_one_'");
    SXEL82I("node=%p, pool=%p", &pool->nodes[0], pool);
    SXEA80I(sxe_pool_index_to_state(pool->nodes, 0) != SXE_POOL_TCP_STATE_UNCONNECTED, "Item 0 is in UNCONNECTED state");

    test_ev_loop_wait(2); /* the close indication */
    test_ev_loop_wait(2); /* accept the new connection */
    is(close_count  , 1,        "One connection has been closed");
    is(connect_count, 1,        "An additional connection was attempted");
    tap_ev_free(tap_ev_shift());
    tap_ev_free(tap_ev_shift());

    sxe_pool_tcp_delete(NULL, pool); /* Should close the sockets created by accept() (listener) */
    test_ev_loop_wait(2);
    test_ev_loop_wait(2);
    is(tap_ev_count("test_event_server_close"), 2, "Confirm that both accept() sockets were closed");
    tap_ev_free(tap_ev_shift());
    tap_ev_free(tap_ev_shift());

    sxe_close(listener);
    sxe_fini();
    test_case_cleanup();
}

static void
test_case_pool_too_big_to_service(void)
{
    SXE_POOL_TCP * big_pool;
    SXE *          listener;
    unsigned short test_port;

    test_case_setup();

    /* Show that we can ask for insane numbers of connections,
     * and that'll work.  This may be silly, but the test is here to
     * demonstrate the current behavior.
     */

    sxe_pool_tcp_register(5);
    is(sxe_init(),                               SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read_and_send, test_event_server_close);
    ok(listener != NULL,                                        "Created a listener");
    is(sxe_listen(listener),                     SXE_RETURN_OK, "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    MOCK_SET_HOOK(connect, test_mock_connect);
    is(close_count,                                          0, "Nothing closed yet");

    big_pool = sxe_pool_tcp_new_connect(20, "bigpool", "127.0.0.1", test_port, test_event_client_ready_to_write,
                                        test_event_client_connected, test_event_client_read, test_event_client_close,
                                        NO_TIMEOUT, NO_TIMEOUT, test_event_timeout, caller_info);
    ok(big_pool != NULL,                                        "Created big pool which apparently contains 20 connections");
    test_ev_loop_wait(2);
    is(connect_count,                                        4, "All 4 of our connections should be either connected or connecting");
    test_ev_loop_wait(2);
    is(connect_count,                                        5, "We should have closed/retried once");
    is(tap_ev_count("test_event_client_connected"),          4, "All attempts to connect were successful (from client's point of view)");
    is(read_length_sum,                                      0, "Only connect events happened (0 length reads)");
    is(close_count,                                          1, "The client saw one closed socket");

    /* Delete the pool, but don't test for close events - we don't care, will never do this in production.
     */
    sxe_pool_tcp_delete(NULL, big_pool);
    sxe_close(listener);
    sxe_fini();
    test_case_cleanup();
}

static void
test_case_max_capacity(void)
{
    SXE_POOL_TCP* pool;
    SXE *         listener;
    char          buf[1024];
    SXE *         sxe_first;
    SXE *         sxe_second;
    unsigned short test_port;

    test_case_setup();
    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read_and_keep_state, test_event_server_close);
    ok(listener != 0, "Created a listener");
    is(sxe_listen(listener), SXE_RETURN_OK,
        "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    MOCK_SET_HOOK(connect, test_mock_connect);
    pool = sxe_pool_tcp_new_connect(2, "max_capacity_pool", "127.0.0.1", test_port, test_event_client_ready_to_write,
                                    test_event_client_connected, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, caller_info);
    ok(pool != NULL, "max_capacity_pool was initialized");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "1st client connected");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "2nd client connected");


    is(connect_count,        2, "There should be two connects made to the listen socket");
    is(read_length_sum,      0, "Read callback was called with zero length for both connections");
    is(ready_to_write_count, 0, "'Ready to Write' event was not called back");

    /*
     * Now we can start writing to the connections we've established
     */
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "max one", 7, buf), SXE_RETURN_OK,
        "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_first = listen_read_last_sxe;

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "max two", 7, buf), SXE_RETURN_OK, "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_second = listen_read_last_sxe;

    is(sxe_pool_get_number_in_state(pool->nodes, SXE_POOL_TCP_STATE_READY_TO_SEND), 0, "We're at max capacity now (concurrency of 2)");

    ready_to_write_count = 0;
    is(sxe_write(sxe_first, "xxx", 3), SXE_RETURN_OK, "Respond acting as the listener");
    SXE_BUF_CLEAR(sxe_first);
    test_ev_loop_wait(2);
    is(ready_to_write_count, 0, "No 'Ready to Write' events happened unnecessarily");

    is(sxe_write(sxe_second, "xxx", 3), SXE_RETURN_OK, "Respond acting as the listener");
    SXE_BUF_CLEAR(sxe_second);
    process_all_libev_events(0);
    is(ready_to_write_count, 0, "No 'Ready to Write' events happened unnecessarily");

    sxe_close(listener);
    sxe_pool_tcp_delete(NULL, pool);
    process_all_libev_events(0);
    sxe_fini();
    test_case_cleanup();
}

static void
test_case_queue_ready_to_write_events(void)
{
    SXE_POOL_TCP* pool;
    SXE *         listener;
    char          buf[1024];
    SXE *         sxe_first;
    SXE *         sxe_second;
    SXE *         sxe_third;
    SXE *         sxe_fourth;
    unsigned short test_port;

    test_case_setup();

    sxe_pool_tcp_register(10);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    listener = sxe_new_tcp(NULL, "INADDR_ANY", 0, NULL, test_event_server_read_and_keep_state, test_event_server_close);
    ok(listener != 0, "Created a listener");
    is(sxe_listen(listener), SXE_RETURN_OK,
        "Listening on a TCP port so that sxe_pool_tcp can make connections to it");
    test_port = SXE_LOCAL_PORT(listener);

    MOCK_SET_HOOK(connect, test_mock_connect);
    pool = sxe_pool_tcp_new_connect(2, "qr2we_pool", "127.0.0.1", test_port, test_event_client_ready_to_write,
                                    test_event_client_connected, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, caller_info);
    ok(pool != NULL, "qr2we_pool was initialized");

    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "1st client connected");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2)), "test_event_client_connected", "2nd client connected");
    is(read_length_sum,                                   0, "Read callback was called with zero length for both connections");
    is(ready_to_write_count,                              0, "'Ready to Write' event was not called back");

    /*
     * Now show that we can write stuff to the connection
     *
     * Note:
     *  - the SXE connections are pool->node_not_ready_to_send[0..1].sxe
     */
    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "queue one", 9, buf), SXE_RETURN_OK, "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_first = listen_read_last_sxe;

    test_check_ready_to_write(pool, 1, __func__, __LINE__);
    is(sxe_pool_tcp_write(pool, "queue two", 9, buf), SXE_RETURN_OK, "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_second = listen_read_last_sxe;

    test_check_ready_to_write(pool, 0, __func__, __LINE__);
    process_all_libev_events(0);

    test_check_ready_to_write(pool, 0, __func__, __LINE__);
    process_all_libev_events(0);

    ready_to_write_count = 0;
    is(sxe_write(sxe_first, "xxx", 3), SXE_RETURN_OK, "respond acting as the listener");
    process_all_libev_events(0);
    is(ready_to_write_count, 1, "'Ready to Write' event due to a new available connection");
    is(sxe_pool_tcp_write(pool, "queue three", 11, buf), SXE_RETURN_OK,
        "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_third = listen_read_last_sxe;

    ready_to_write_count = 0;
    is(sxe_write(sxe_second, "xxx", 3), SXE_RETURN_OK, "respond acting as the listener");
    process_all_libev_events(0);
    is(ready_to_write_count, 1, "Another 'Ready to Write' event due to a another new available connection");
    is(sxe_pool_tcp_write(pool, "queue four", 10, buf), SXE_RETURN_OK,
        "The 'Ready to Write' event signified we could write, and we did");
    process_all_libev_events(0);
    sxe_fourth = listen_read_last_sxe;

    ready_to_write_count = 0;
    is(sxe_write(sxe_third, "xxx", 3), SXE_RETURN_OK, "Respond acting as the listener");
    process_all_libev_events(0);
    is(ready_to_write_count, 0, "No 'Ready to Write' events happened unnecessarily");

    is(sxe_write(sxe_fourth, "xxx", 3), SXE_RETURN_OK, "Respond acting as the listener");
    process_all_libev_events(0);
    is(ready_to_write_count, 0, "No 'Ready to Write' events happened unnecessarily");

    sxe_close(listener);
    sxe_pool_tcp_delete(NULL, pool);
    process_all_libev_events(0);
    sxe_fini();
    test_case_cleanup();
}

static void
test_case_queue_ready_to_write_events_on_connection(void)
{
    SXE_POOL_TCP*        pool;
    unsigned             i;

    test_case_setup();
    sxe_pool_tcp_register(4);
    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    MOCK_SET_HOOK(connect, test_mock_connect);
    pool = sxe_pool_tcp_new_connect(2, "ready_pool", "127.0.0.1", 8009, test_event_client_ready_to_write,
                                    test_event_client_connected, test_event_client_read, test_event_client_close, NO_TIMEOUT,
                                    NO_TIMEOUT, test_event_timeout, caller_info);
    ok(pool != NULL, "ready_pool was initialized");

    ok(connect_count       > 0, "No listenter, but there will be attempts to connect");

    test_check_ready_to_write(pool, 0, __func__, __LINE__);
    process_all_libev_events(0);
    test_check_ready_to_write(pool, 0, __func__, __LINE__);
    process_all_libev_events(0);

    /* 4 closes, one per connection and one extra retry specified by SXE_POOL_TCP_FAILURE_MAX hardcoded in sxe-pool-tcp.c */
    for (i = 1 ; i <= 4 ; ++i) {
        is_eq(tap_ev_identifier(test_tap_ev_shift_wait(2.0)), "test_event_client_close", "Close event %u of 4", i);
    }

    sxe_pool_tcp_delete(NULL, pool);
    process_all_libev_events(0);
    sxe_fini();
    test_case_cleanup();
}

int main(void)
{
    plan_tests(121);
    test_case_happy_path();
    test_case_connection_closed_and_reopened();
    test_case_pool_too_big_to_service();
    test_case_max_capacity();
    test_case_queue_ready_to_write_events();
    test_case_queue_ready_to_write_events_on_connection();
    return exit_status();
}

