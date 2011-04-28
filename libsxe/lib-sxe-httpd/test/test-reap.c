#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#define TEST_WAIT 2

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
    tap_ev_queue_push(tq_server, __func__, 2, "request", request, "this", this);
    SXER60I("return");
}

static void
evhttp_request(struct SXE_HTTPD_REQUEST *request, const char *method, unsigned mlen, const char *url, unsigned ulen, const char *version, unsigned vlen)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(method);
    SXE_UNUSED_PARAMETER(mlen);
    SXE_UNUSED_PARAMETER(url);
    SXE_UNUSED_PARAMETER(ulen);
    SXE_UNUSED_PARAMETER(version);
    SXE_UNUSED_PARAMETER(vlen);
    SXEE67I("%s(method=[%.*s],url=[%.*s],version=[%.*s])", __func__, mlen, method, ulen, url, vlen, version);
    tap_ev_queue_push(tq_server, __func__, 2, "request", request, "this", this);
    SXER60I("return");
}

static void
evhttp_close(SXE_HTTPD_REQUEST *request)
{
    SXE *this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE62I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 2, "request", request, "this", this);
    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD    httpd;
    SXE        * client;
    SXE        * client2;
    SXE        * server;
    SXE        * server2;
    tap_ev       event;

    plan_tests(13);;

    sxe_register(1000, 0);
    tq_client = tap_ev_queue_new();
    tq_server = tap_ev_queue_new();

    SXEA10(sxe_init() == SXE_RETURN_OK,                                                          "Failed to initialize SXE package");

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, evhttp_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, request, evhttp_request);
    SXE_HTTPD_SET_HANDLER(&httpd, close, evhttp_close);
    ok((listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL,                              "HTTPD listening");

    /* Starts two connections, then make a new connection, the oldest will be reaped by the server */
    tap_test_case_name("reap connections");

    /* 1st connection */
    SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
           "Failed to allocate client SXE");
    SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,          "Failed to connect to HTTPD");
    is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got 1st client connected event");
    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got 1st server connect event");
    server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

    /* 1st client: send a complete request line */
    SXEA10(SXE_WRITE_LITERAL(client, "GET /good HTTP/1.1\r\n") == SXE_RETURN_OK,             "Failed to write good request");
    test_process_all_libev_events();

    /* Waiting for 1st client request state "stable" on "STATE_HEADER" to make
     * sure there is no more state updates on this request object, then the following
     * usleep will guarantee the 1st connection is older than the 2nd one.
     */
    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_request",     "Got 1st server request event");
    is(tap_ev_arg(event, "this"), server,                                                        "It's the 1st server");
    usleep(300000);

    /* 2nd connection, reaping happens, the 1st one got reaped */
    SXEA10((client2 = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
           "Failed to allocate client SXE");
    SXEA10(sxe_connect(client2, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,         "Failed to connect to HTTPD");
    is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got 2nd client connected event");
    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect",     "Got 2nd server connect event");
    server2 = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

    /* 2nd client: send a non-complete request line to not trigger "on_request" server event*/
    SXEA10(SXE_WRITE_LITERAL(client2, "GET /good HTTP/1.1") == SXE_RETURN_OK,                    "Failed to write good request");

    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_close",       "Got a server close event");
    is(tap_ev_arg(event, "this"), server,                                                        "It's the 1st server");
    is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_close",   "Got a cient close event");
    is(tap_ev_arg(event, "this"), client,                                                        "It's the 1st client");

    is(tap_ev_queue_length(tq_server),                                         0,                "No server events lurking");
    is(tap_ev_queue_length(tq_client),                                         0,                "No client events lurking");

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
