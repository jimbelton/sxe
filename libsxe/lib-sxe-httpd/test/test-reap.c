#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#include "common.h"

#define TEST_WAIT 2

static SXE        * listener;

int
main(void)
{
    SXE_HTTPD    httpd;
    SXE        * client;
    SXE        * client2;
    SXE        * server;
    tap_ev       event;

    plan_tests(14);

    test_sxe_register_and_init(1000);

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, h_request);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   h_close);
    ok((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                              "HTTPD listening");

    /* Starts two connections, then make a new connection, the oldest will be reaped by the server */
    tap_test_case_name("reap connections");

    /* 1st connection */
    SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
           "Failed to allocate client SXE");
    SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,          "Failed to connect to HTTPD");
    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect", "Got 1st client connected event");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_connect",     "Got 1st server connect event");
    server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

    /* 1st client: send a complete request line */
    TEST_SXE_SEND_LITERAL(client, "GET /good HTTP/1.1\r\n", client_sent, q_client, TEST_WAIT, &event);

    /* Waiting for 1st client request state "stable" on "STATE_HEADER" to make
     * sure there is no more state updates on this request object, then the following
     * usleep will guarantee the 1st connection is older than the 2nd one.
     */
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_request",     "Got 1st server request event");
    is(tap_ev_arg(event, "this"), server,                                                        "It's the 1st server");
    usleep(300000);

    /* 2nd connection, reaping happens, the 1st one got reaped */
    SXEA1((client2 = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
           "Failed to allocate client SXE");
    SXEA1(sxe_connect(client2, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,         "Failed to connect to HTTPD");

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_connect",     "Got 2nd server connect event");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_close",       "Got a server close event");
    is(tap_ev_arg(event, "this"), server,                                                        "It's the 1st server");

    /* Pull these two arguments off in any order */
    test_process_all_libev_events();
    ok((event = tap_ev_queue_shift_next(q_client, "client_connect")) != NULL, "Got 2nd client connected event");
    ok((event = tap_ev_queue_shift_next(q_client, "client_close")) != NULL, "Got a client close event");
    is(tap_ev_arg(event, "this"), client,                                                        "It's the 1st client");

    is(tap_ev_queue_length(q_httpd),                                         0,                "No server events lurking");
    is(tap_ev_queue_length(q_client),                                         0,                "No client events lurking");

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
