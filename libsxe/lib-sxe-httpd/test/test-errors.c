#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#include "common.h"

#define TEST_WAIT 2
#define TEST_400  "HTTP/1.1 400 Bad request\r\n"           "Connection: close\r\n" "Content-Length: 0\r\n\r\n"
#define TEST_414  "HTTP/1.1 414 Request-URI too large\r\n" "Connection: close\r\n" "Content-Length: 0\r\n\r\n"
#define A100      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define A1000     A100 A100 A100 A100 A100 A100 A100 A100 A100 A100
#define TEST_CASES (sizeof(cases)/sizeof(cases[0]))

/* Table of error cases
 *
 * Note: Removed { "GET / HTTP/1.0\r\nA:\r\n\r\n", TEST_400 }, which is a valid message with an empty value for header 'A'
 */
static struct {
    const char *request;
    const char *response;
} cases[] = {
    { "\r\n",                                                   TEST_400 },
    { "GET \r\n",                                               TEST_400 },
    { "GET /\r\n",                                              TEST_400 },
    { "GET / HTTP/9.9\r\n",                                     TEST_400 },
    { "GET / HTTP/1.1 \n",                                      TEST_400 },
    { "FIGZZ / HTTP/1.1\r\n\r\n",                               TEST_400 },
    { "GET / HTTP/1.0\r\nSDF\r\n\r\n",                          TEST_400 },
    { "GET / HTTP/1.0\r\nA B\r\n\r\n",                          TEST_400 },
    { "GET / HTTP/1.0\r\n:B\r\n\r\n",                           TEST_400 },
    { "GET / HTTP/1.0\r\n A:B\r\n\r\n",                         TEST_400 },
    { "POST / HTTP/1.1\r\nContent-Length:\r\n\r\n",             TEST_400 },    /* Empty Content-Length */
    { "POST / HTTP/1.1\r\nContent-Length: non-numeric\r\n\r\n", TEST_400 },
    { A1000 A1000,                                              TEST_414 },
};

static SXE        * listener;
static tap_ev_queue tq_server;

static void
evhttp_connect(SXE_HTTPD_REQUEST * request)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER6I("return");
}

static void
evhttp_close(SXE_HTTPD_REQUEST * request)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER6I("return");
}

static void
evhttp_respond(SXE_HTTPD_REQUEST * request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE6I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER6I("return");
}

//static void
//evhttp_request(SXE_HTTPD_REQUEST * request, const char * method, unsigned method_length, const char * url, unsigned url_length,
//               const char * version, unsigned version_length)
//{
//    SXE *this = request->sxe;
//    SXE_UNUSED_PARAMETER(this);
//    SXEE6I("%s(request=%p)", __func__, request);
//    tap_ev_queue_push(tq_server, __func__, 7, "request"       , request       ,
//                                              "method"        , method        ,
//                                              "method_length" , method_length ,
//                                              "url"           , url           ,
//                                              "url_length"    , url_length    ,
//                                              "version"       , version       ,
//                                              "version_length", version_length);
//    SXER6I("return");
//}

int
main(void)
{
    SXE_HTTPD    httpd;
    SXE        * client;
    SXE        * client2;
    tap_ev       event;
    const char * request;
    const char * response;
    unsigned     id;
    char         buffer[4096];
    char         test_case_name[8];

    tap_plan(25 + (9 * TEST_CASES), TAP_FLAG_ON_FAILURE_EXIT, NULL);

    //putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTP=7");
    //putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTPD=5"); /* comment this line to debug this test */
    test_sxe_register_and_init(1000);
    tq_server = tap_ev_queue_new();

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, evhttp_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, close,   evhttp_close);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, evhttp_respond);
    ok((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                              "HTTPD listening");

    /* Make sure you get the FREE state on a client close
     */
    tap_test_case_name("free on close");
    {
        SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
               "Failed to allocate client SXE");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,           "Failed to connect to HTTPD");
        is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect",      "Got a client connected event");
        is(tap_ev_arg(event, "this"), client,                                                        "It's the client");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got a server connected event");
        sxe_close(client);
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_close",       "Got a server close event");
        is(tap_ev_queue_length(tq_server),                                     0,                    "No server events lurking");
    }

    /* Send a bad request, make sure an error response occurs, send another request (which should be served this time), then
     * make a new connection and make sure the previous one is reaped by the server (it is the oldest one and a free one is required)
     */
    tap_test_case_name("reap connections");
    {
        SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
               "Failed to allocate client SXE");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,           "Failed to connect to HTTPD");
        is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect",      "Got a client connected event");
        is(tap_ev_arg(event, "this"), client,                                                        "It's the client");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got a server connect event");
        TEST_SXE_SEND_LITERAL(client, "GISSLERB\r\n", client_sent, q_client, TEST_WAIT, &event);

        test_ev_queue_wait_read(q_client, TEST_WAIT, &event, client, "client_read", buffer, SXE_LITERAL_LENGTH(TEST_400), "client");
        is_strncmp(buffer, TEST_400, SXE_LITERAL_LENGTH(TEST_400),                                   "Got correct error response");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_close",       "httpd indicates error via close callback");

        /* Anymore writes to this connection are ignored */
        TEST_SXE_SEND_LITERAL(client, "GET /good HTTP/1.1\r\n\r\n", client_sent, q_client, TEST_WAIT, &event);
        is(tap_ev_queue_length(tq_server),                                         0,                "No server events lurking");
        is(tap_ev_queue_length(q_client),                                          0,                "No client events lurking");

        SXEA1((client2 = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
               "Failed to allocate a second client SXE");
        SXEA1(sxe_connect(client2, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,          "Failed to connect to HTTPD");

        if (strcmp(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect") == 0) {
            pass(                                                                                    "Got a client connected event");
            is(tap_ev_arg(event, "this"), client2,                                                   "It's the 2nd client");
            is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_close",    "Got a client closed event");
            is(tap_ev_arg(event, "this"), client,                                                    "It's the 1st (reaped) client");
        }
        else {
            is_eq(tap_ev_identifier(event),                                        "client_close",   "Got a client closed event");
            is(tap_ev_arg(event, "this"), client,                                                    "It's the 1st (reaped) client");
            is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect",  "Got a client connected event");
            is(tap_ev_arg(event, "this"), client2,                                                   "It's the 2nd client");
        }

        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_connect", "Got a server connect event");

        sxe_close(client2);
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_close",   "Got a server close event for client 2");
        is(tap_ev_queue_length(tq_server),                                         0,                "No server events lurking");
        is(tap_ev_queue_length(q_client),                                          0,                "No client events lurking");
    }

    for (id = 0; id < TEST_CASES; id++) {
        snprintf(test_case_name, sizeof(test_case_name), "Case %u", id);
        tap_test_case_name(test_case_name);
        diag("length of server queue = %u", tap_ev_queue_length(tq_server));

        SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
               "Failed to allocate client SXE");
        SXE_USER_DATA_AS_INT(client) = id;
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,           "Failed to connect to HTTPD");
#ifdef _WIN32
        usleep(10000);    /* TODO: Figure out why this delay is required */
#endif
        is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect",      "Got a client connected event");
        is(tap_ev_arg(event, "this"), client,                                                        "It's the client");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got a server connected event");
        request = cases[SXE_USER_DATA_AS_INT(client)].request;
        test_sxe_send(client, request, strlen(request), client_sent, "client_sent", q_client, TEST_WAIT, &event);
        response = cases[id].response;
        test_ev_queue_wait_read(q_client, TEST_WAIT, &event, client, "client_read", buffer, strlen(response), "client");
        is_strncmp(buffer, response, strlen(response),                                               "Got correct response");
        sxe_close(client);

        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_close",   "Got a server close event");

        is(tap_ev_queue_length(tq_server),                                         0,                "No server events lurking");
        is(tap_ev_queue_length(q_client),                                          0,                "No client events lurking");
    }

    tap_test_case_name("closed before accept event is handled");
    {
#ifdef _WIN32
        skip(1, "Unix specific test");
#else
#if ENABLE_SSL
        SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
               "Failed to allocate client SXE");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to connect to HTTPD");
        ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_ONESHOT);
        sxe_close(client);
        is(tap_ev_queue_length(tq_server),                                         0,                "No events on failed SSL handshake");
#else
        skip(1, "SSL-specific tests");
#endif
#endif
    }

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
