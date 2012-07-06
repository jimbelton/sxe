#include "common.h"
#include "sxe.h"
#include "sxe-httpd.h"
#include "tap.h"

#define TEST_WAIT 2

static SXE        * listener;

static void
test_event_eoh(struct SXE_HTTPD_REQUEST *request)
{
    SXE * this = sxe_httpd_request_get_sxe(request);

    SXE_UNUSED_PARAMETER(this);
    SXEE6I("()");
    tap_ev_queue_push(q_httpd, __func__, 1, "request", request);
    sxe_httpd_pause(request);
    SXER6I("return");
}

int
main(void)
{
    SXE_HTTPD httpd;
    SXE     * client;
    tap_ev    event;

    plan_tests(9);
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE=6");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTP=5");
//    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTPD=6"); /* comment this line to debug this test */

    test_sxe_register_and_init(1000);

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, header,  h_header);
    SXE_HTTPD_SET_HANDLER(&httpd, eoh,     test_event_eoh);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   h_close);
    ok((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL, "HTTPD listening");

    SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL, "Failed to allocate client SXE");
    SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,                    "Failed to connect to HTTPD");

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect", "Got a client connected event");
    is(tap_ev_arg(event, "this"),                                         client,           "It's the client"             );
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_connect",       "Got a server connected event");

#   define TEST_REQUEST "GET / HTTP/1.0\r\n\r\n"
    test_sxe_send(client, TEST_REQUEST, strlen(TEST_REQUEST), client_sent, "client_sent", q_client, TEST_WAIT, &event);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "test_event_eoh",  "HTTPD eoh event");
    test_process_all_libev_events();
    is(tap_ev_queue_length(q_httpd), 0,                                                     "No more events received");

    sxe_httpd_resume(SXE_CAST_NOCONST(SXE_HTTPD_REQUEST *, tap_ev_arg(event, "request")));
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_respond",       "HTTPD respond event");
    sxe_close(client);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_close",         "HTTPD close event");

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
