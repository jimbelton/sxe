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
#include "sxe-pool-tcp.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "tap.h"

#ifdef WINDOWS_NT
#else

static const unsigned short test_port = 8086;

static unsigned int   connect_count                = 0;
static unsigned int   getsockopt_count             = 0;
static unsigned int   ready_to_write_count         = 0;
static unsigned int   close_count                  = 0;
static double         response_timeout             = 0.0;    /* 0.0 means infinite */
static void         * caller_info                  = NULL;

double time_to_wait_for_windows_to_close_on_a_bad_connect = 10.0;

static int SXE_STDCALL
test_mock_connect(SXE_SOCKET sockfd, const struct sockaddr *serv_addr, SXE_SOCKLEN_T addrlen)
{
    int result = 0;
    SXEE6("test_mock_connect(sockfd=%d, serv_addr=%p, addrlen=%d)", sockfd, serv_addr, addrlen);
    SXE_UNUSED_PARAMETER(sockfd);
    SXE_UNUSED_PARAMETER(serv_addr);
    SXE_UNUSED_PARAMETER(addrlen);
    ++connect_count;

    /* Call the real connect function.
     */
    MOCK_SET_HOOK(connect, connect);
    result = connect(sockfd, serv_addr, addrlen);
    MOCK_SET_HOOK(connect, test_mock_connect);

    SXER6("return %d", result);
    return result;
}

static int SXE_STDCALL
test_mock_getsockopt_fails(SXE_SOCKET fd, int level, int optname, SXE_SOCKET_VOID * optval, SXE_SOCKLEN_T * SXE_SOCKET_RESTRICT optlen)
{
    SXEE6("test_mock_getsockopt_fails(fd=%d, level=%d, optname=%d, optval=%p, optlen=%p)", fd, level, optname, optval, optlen);
    SXE_UNUSED_PARAMETER(fd);
    SXE_UNUSED_PARAMETER(level);
    SXE_UNUSED_PARAMETER(optname);
    SXE_UNUSED_PARAMETER(optval);
    SXE_UNUSED_PARAMETER(optlen);
    ++getsockopt_count;
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EBADF));
    SXER6("return -1");
    return -1;
}

static int SXE_STDCALL
test_mock_getsockopt_connection_failed(SXE_SOCKET fd, int level, int optname, SXE_SOCKET_VOID * optval, SXE_SOCKLEN_T * SXE_SOCKET_RESTRICT optlen)
{
    SXEE6("test_mock_getsockopt_connection_failed(fd=%d, level=%d, optname=%d, optval=%p, optlen=%p)", fd, level, optname, optval, optlen);
    SXE_UNUSED_PARAMETER(fd);
    SXE_UNUSED_PARAMETER(level);
    SXE_UNUSED_PARAMETER(optname);
    ++getsockopt_count;
    SXEA1(level == SOL_SOCKET, "getsockopt() expects level to be SOL_SOCKET");
    SXEA1(optname == SO_ERROR, "getsockopt() expects option to be SO_ERROR");
    SXEA1(*optlen >= 4, "getsockopt() must be passed at least four bytes");
    *(unsigned*)(optval) = SXE_SOCKET_ERROR(ECONNREFUSED);
    *optlen = 4;

    SXER6("return 0 (ECONNREFUSED as passed via parameters)");
    return 0;
}
static void
test_event_timeout(SXE_POOL_TCP * pool, void * info)
{
    SXEE6("%s(pool=%s, info=%p)", __func__, SXE_POOL_TCP_GET_NAME(pool), info);
    SXE_UNUSED_PARAMETER(pool);
    SXE_UNUSED_PARAMETER(info);
    SXEA1(0, "test_event_timeout should never get called in this test");
    SXER6("return");
}

static void
test_event_ready_to_write(SXE_POOL_TCP * pool, void * info)
{
    SXE_UNUSED_PARAMETER(pool);
    SXE_UNUSED_PARAMETER(info);
    SXEE6("test_event_ready_to_write(pool=%s, info=%p)", SXE_POOL_TCP_GET_NAME(pool), info);
    ++ready_to_write_count;
    SXER6("return");
}

static void
test_event_read(SXE* this, int length)
{
    SXEE6I("test_event_read(length=%d)", length);
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(length);
    SXEA1(0, "test_event_read should never get called in this test");
    SXER6I("return");
}

static void
test_event_close(SXE* this)
{
    SXEE6I("test_event_close()");
    SXE_UNUSED_PARAMETER(this);
    ++close_count;
    SXER6I("return");
}

static void
process_all_libev_events(void)
{
    SXEE6("process_all_libev_events()");

    /* TODO: work out if there's a better way of processessing all outstanding
     * libev events, including new events which are generated by our callbacks
     * */
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    SXER6("return");
    return;
}

static void
check_invalid_destination(void)
{
    SXE_POOL_TCP * pool;
    double         deadline = 0.0;

    SXEE6("check_invalid_destination()");
    connect_count = 0;
    close_count   = 0;
    sxe_pool_tcp_register(10);

    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    /* NOTE: We are deliberately connecting to an invalid port
     */
    connect_count = 0;
    deadline = sxe_get_time_in_seconds() + time_to_wait_for_windows_to_close_on_a_bad_connect;
    MOCK_SET_HOOK(connect, test_mock_connect);
    getsockopt_count = 0;
    MOCK_SET_HOOK(getsockopt, test_mock_getsockopt_connection_failed);
    pool = sxe_pool_tcp_new_connect(2, "destpool", "127.0.0.1", test_port, test_event_ready_to_write, NULL, test_event_read, test_event_close, 0.0, response_timeout, test_event_timeout, caller_info);
    ok(pool != NULL, "destpool was initialized");

    while (getsockopt_count < 2) {
        if (sxe_get_time_in_seconds() > deadline) {
            SXEL6("Deadline exceeded when waiting for getsockopt() (the connect callback isn't being called)");
            break;
        }

        test_ev_loop_wait(2);
    }

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);

    /* TODO: lib-sxe-tcp-pool will brainlessly keep trying to open connections which fail.  We should introduce some delays when retrying */
    ok(connect_count      >= 2, "There should be at least two connect attempts");
    ok(getsockopt_count   >= 2, "There should be at least two calls to getsockopt()");

    process_all_libev_events();
    ok(close_count        >= 2, "Connection was closed, and our close callback was processed");
    ready_to_write_count = 0;

    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is(ready_to_write_count, 0, "no connections established so no write events");

    process_all_libev_events();    /* Make sure there are no spurious read events */
    MOCK_SET_HOOK(connect, connect);
    sxe_pool_tcp_delete(NULL, pool);
    sxe_fini();

    SXER6("return");
}

static void
check_getsockopt_failure(void)
{
    SXE_POOL_TCP * pool;
    double         deadline = 0.0;

    SXEE6("check_getsockopt_failure()");
    connect_count   = 0;
    close_count     = 0;
    sxe_pool_tcp_register(10);

    is(sxe_init(), SXE_RETURN_OK, "Initialized SXE");

    /* NOTE: We are deliberately connecting to an invalid port
     */
    connect_count = 0;
    deadline = sxe_get_time_in_seconds() + time_to_wait_for_windows_to_close_on_a_bad_connect;
    MOCK_SET_HOOK(connect, test_mock_connect);
    getsockopt_count = 0;
    MOCK_SET_HOOK(getsockopt, test_mock_getsockopt_fails);
    pool = sxe_pool_tcp_new_connect(2, "sockpool", "127.0.0.1", test_port, test_event_ready_to_write, NULL, test_event_read, test_event_close, 0.0, response_timeout, test_event_timeout, caller_info);
    ok(pool != NULL, "sockpool was initialized");

    while (getsockopt_count < 2) {
        if (sxe_get_time_in_seconds() > deadline) {
            SXEL6("Deadline exceeded when waiting for getsockopt() (the connect callback isn't being called)");
            break;
        }

        ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_ONESHOT);
    }

    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    ok(connect_count      >= 2, "There should be at least two connect attempts");
    ok(getsockopt_count   >= 2, "There should be at least two calls to getsockopt()");

    process_all_libev_events();
    ok(close_count        >= 2, "Connection was closed, and our close callback was processed");

    ready_to_write_count = 0;
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is(ready_to_write_count, 0, "no connections established so no write events");

    process_all_libev_events();    /* Make sure there are no spurious read events */
    MOCK_SET_HOOK(connect,    connect);
    MOCK_SET_HOOK(getsockopt, getsockopt);
    sxe_pool_tcp_delete(NULL, pool);
    pool = NULL;
    sxe_fini();
    SXER6("return");
}

#endif

int
main(void)
{
#ifdef WINDOWS_NT
    diag("TODO: sxe-pool-tcp connection failure test cases are disabled on Windows (probable lib-ev on Windows bug)\n");
    return 0;
#else
    plan_tests(12);

    check_invalid_destination();
    check_getsockopt_failure();

    return exit_status();
#endif
} /* main() */

