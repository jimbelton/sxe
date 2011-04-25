#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#define TEST_WAIT 5

#define A100      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define A1000     A100  A100 A100 A100 A100 A100 A100 A100 A100 A100
#define A1500     A1000 A100 A100 A100 A100 A100
#define A16000    A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000

#define REQUEST_LINE   "GET / HTTP/1.0\r\n"
#define GOOD_HEADER    "Good: Header\r\n"
#define ANOTHER_HEADER "Another: Foo\r\n"
#define IGNORED_HEAD   "Really-Big: " A16000 A1000 "\r\n"

#define IGNORED_HEAD_2   "H15H: " A1500 "\r\n"

#define TEST_CASES (sizeof(cases)/sizeof(cases[0]))

static SXE        * listener;
static tap_ev_queue tq_client;
static tap_ev_queue tq_server;

static struct {
    const char *request;
    const char *tapname;
    unsigned   header_count;
} cases[] = {
    { REQUEST_LINE GOOD_HEADER  IGNORED_HEAD   ANOTHER_HEADER "\r\n", "Good-Big-Another", 2 },
    { REQUEST_LINE IGNORED_HEAD GOOD_HEADER    ANOTHER_HEADER "\r\n", "Big-Good-Another", 2 },
    { REQUEST_LINE GOOD_HEADER  ANOTHER_HEADER IGNORED_HEAD   "\r\n", "Good-Another-Big", 2 },
    { REQUEST_LINE IGNORED_HEAD_2                             "\r\n", "Big(~1500)",       0 },
};

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

static void
evhttp_respond(SXE_HTTPD_REQUEST *request)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE62I("%s(request=%p)", __func__, request);
    tap_ev_queue_push(tq_server, __func__, 1, "request", request);
    SXER60I("return");
}

static void
evhttp_header(struct SXE_HTTPD_REQUEST *request, const char *key, unsigned klen, const char *val, unsigned vlen)
{
    SXE * this = request->sxe;
    SXE_UNUSED_PARAMETER(this);
    SXEE65I("%s(key=[%.*s],value=[%.*s])", __func__, klen, key, vlen, val);
    tap_ev_queue_push(tq_server, __func__, 3,
                      "request", request,
                      "key", tap_dup(key, klen),
                      "value", tap_dup(val, vlen));
    SXER60I("return");
}

static void
test_case(const char * tapname, const char * request, unsigned count)
{
    SXE  * client;
    tap_ev event;

    tap_test_case_name(tapname);

    SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
            "Failed to allocate client SXE");
    SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to connect to HTTPD");

#ifdef _WIN32
    usleep(10000);    /* Copied from test-errors.c and which is a TODO item */
#endif

    is_eq(test_tap_ev_queue_identifier_wait(tq_client, TEST_WAIT, &event), "test_event_connect", "Got a client connected event");
    is(tap_ev_arg(event, "this"), client, "It's the client");
    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_connect", "Got a server connected event");

    SXEA10(sxe_write(client, request, strlen(request)) == SXE_RETURN_OK, "Failed to write the long request");

    if (count == 2) {
        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_header", "HTTPD: header event");
        is_strncmp(tap_ev_arg(event, "key"), "Good", SXE_LITERAL_LENGTH("Good"), "HTTPD: header was 'Good'");
        is_strncmp(tap_ev_arg(event, "value"), "Header", SXE_LITERAL_LENGTH("Header"), "HTTPD: header value was 'Header'");

        is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_header", "HTTPD: header event");
        is_strncmp(tap_ev_arg(event, "key"), "Another", SXE_LITERAL_LENGTH("Another"), "HTTPD: header was 'Another'");
        is_strncmp(tap_ev_arg(event, "value"), "Foo", SXE_LITERAL_LENGTH("Foo"), "HTTPD: header value was 'Foo'");
    }

    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_respond", "HTTPD: respond event");

    sxe_close(client);
    is_eq(test_tap_ev_queue_identifier_wait(tq_server, TEST_WAIT, &event), "evhttp_close", "HTTPD: close event");
}


int
main(void)
{
    SXE_HTTPD httpd;
    unsigned  id;

    plan_tests(39);

    sxe_register(1000, 0);
    tq_client = tap_ev_queue_new();
    tq_server = tap_ev_queue_new();

    SXEA10(sxe_init() == SXE_RETURN_OK, "Failed to initialize SXE package");

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, evhttp_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, header,  evhttp_header);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, evhttp_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close, evhttp_close);
    ok((listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL, "HTTPD listening");

    for (id = 0; id < TEST_CASES; id++) {
        test_case(cases[id].tapname, cases[id].request, cases[id].header_count);
    }

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
