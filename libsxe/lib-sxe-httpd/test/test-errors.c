#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

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
static tap_ev_queue tq_client;
static tap_ev_queue tq_server;

static void
test_event_connect(SXE * this)
{
    SXEE61I("test_event_connect(id=%u)", SXE_USER_DATA_AS_INT(this));
    tap_ev_queue_push(tq_client, __func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE63I("test_event_read(id=%u, length=%u): response length %u", SXE_USER_DATA_AS_INT(this), length, SXE_BUF_USED(this));
    tap_ev_queue_push(tq_client, __func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    sxe_buf_clear(this);
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE61I("test_event_close(id=%u)", SXE_USER_DATA_AS_INT(this));
    tap_ev_queue_push(tq_client, __func__, 1, "this", this);
    SXER60I("return");
}

static void
evhttp_connect(SXE_HTTPD_REQUEST *request)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE62I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER60I("return");
}

static void
evhttp_close(SXE_HTTPD_REQUEST *request)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE62I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER60I("return");
}

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

    tap_plan(20 + (8 * TEST_CASES), TAP_FLAG_ON_FAILURE_EXIT, NULL);

    sxe_register(1000, 0);
    tq_client = tap_ev_queue_new();
    tq_server = tap_ev_queue_new();

    SXEA10(sxe_init() == SXE_RETURN_OK,                                                          "Failed to initialize SXE package");

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, evhttp_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, close, evhttp_close);
    ok((listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                              "HTTPD listening");

    /* Make sure you get the FREE state on a client close
     */
    tap_test_case_name("free on close");
    {
        SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
               "Failed to allocate client SXE");
        SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,          "Failed to connect to HTTPD");
        is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got a client connected event");
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
        SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
               "Failed to allocate client SXE");
        SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,          "Failed to connect to HTTPD");
        is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got a client connected event");
        is(tap_ev_arg(event, "this"), client,                                                        "It's the client");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got a server connect event");
        SXEA10(SXE_WRITE_LITERAL(client, "GISSLERB\r\n") == SXE_RETURN_OK,                           "Failed to write GISSLERB request");

        test_ev_queue_wait_read(tq_client, TEST_WAIT, &event, client, "test_event_read", buffer, SXE_LITERAL_LENGTH(TEST_400), "client");

        is_strncmp(buffer, TEST_400, SXE_LITERAL_LENGTH(TEST_400),                                   "Got correct error response");

        /* Anymore writes to this connection are ignored */
        SXEA10(SXE_WRITE_LITERAL(client, "GET /good HTTP/1.1\r\n\r\n") == SXE_RETURN_OK,             "Failed to write good request");
        is(tap_ev_queue_length(tq_server),                                         0,                "No server events lurking");
        is(tap_ev_queue_length(tq_client),                                         0,                "No client events lurking");

        SXEA10((client2 = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
               "Failed to allocate a second client SXE");
        SXEA10(sxe_connect(client2, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,         "Failed to connect to HTTPD");

        if (strcmp(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect") == 0) {
            pass(                                                                                        "Got a client connected event");
            is(tap_ev_arg(event, "this"), client2,                                                       "It's the 2nd client");
            is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_close",   "Got a client closed event");
            is(tap_ev_arg(event, "this"), client,                                                        "It's the 1st (reaped) client");
        }
        else {
            is_eq(tap_ev_identifier(event),                                        "test_event_close",   "Got a client closed event");
            is(tap_ev_arg(event, "this"), client,                                                        "It's the 1st (reaped) client");
            is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got a client connected event");
            is(tap_ev_arg(event, "this"), client2,                                                       "It's the 2nd client");
        }

        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_connect",     "Got a server connect event");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_close",       "Got a server close event");

        sxe_close(client2);
       // is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_close",       "Got a server close event");
       // is(tap_ev_queue_length(tq_server),                                         0,                    "No server events lurking");
        is(tap_ev_queue_length(tq_client),                                         0,                    "No client events lurking");
    }

    for (id = 0; id < TEST_CASES; id++) {
        snprintf(test_case_name, sizeof(test_case_name), "Case %u", id);
        tap_test_case_name(test_case_name);
        diag("length of server queue = %u", tap_ev_queue_length(tq_server));

        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event),     "evhttp_close",       "Got a server close event");

        SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
               "Failed to allocate client SXE");
        SXE_USER_DATA_AS_INT(client) = id;
        SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,              "Failed to connect to HTTPD");
#ifdef _WIN32
        usleep(10000);    /* TODO: Figure out why this delay is required */
#endif
        is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect",     "Got a client connected event");
        is(tap_ev_arg(event, "this"), client,                                                            "It's the client");
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",         "Got a server connected event");
        request = cases[SXE_USER_DATA_AS_INT(client)].request;
        SXEA11(sxe_write(client, request, strlen(request)) == SXE_RETURN_OK,                             "Failed to write request %u", id);
        response = cases[id].response;
        test_ev_queue_wait_read(tq_client, TEST_WAIT, &event, client, "test_event_read", buffer, strlen(response), "client");
        is_strncmp(buffer, response, strlen(response),                                                   "Got correct response");
        sxe_close(client);

        is(tap_ev_queue_length(tq_server),                                         0,                    "No server events lurking");
        is(tap_ev_queue_length(tq_client),                                         0,                    "No client events lurking");
    }

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
