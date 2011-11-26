#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#include "common.h"

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
test_case(const char * tapname, const char * request, unsigned count)
{
    SXE  * client;
    tap_ev event;

    tap_test_case_name(tapname);

    SXEA1((client = test_new_tcp(NULL, "0.0.0.0", 0, client_connect, client_read, client_close)) != NULL,
            "Failed to allocate client SXE");
    SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to connect to HTTPD");

#ifdef _WIN32
    usleep(10000);    /* Copied from test-errors.c and which is a TODO item */
#endif

    is_eq(test_tap_ev_queue_identifier_wait(q_client, TEST_WAIT, &event), "client_connect", "Got a client connected event");
    is(tap_ev_arg(event, "this"), client, "It's the client");
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_connect", "Got a server connected event");

    test_sxe_send(client, request, strlen(request), client_sent, "client_sent", q_client, TEST_WAIT, &event);

    if (count == 2) {
        is_eq(test_tap_ev_queue_identifier_wait(q_httpd , TEST_WAIT, &event), "h_header", "HTTPD: header event (a)"         );
        is_strncmp(tap_ev_arg(event, "key"  ), "Good"   , SXE_LITERAL_LENGTH( "Good"   ), "HTTPD: header was 'Good'"        );
        is_strncmp(tap_ev_arg(event, "value"), "Header" , SXE_LITERAL_LENGTH( "Header" ), "HTTPD: header value was 'Header'");

        is_eq(test_tap_ev_queue_identifier_wait(q_httpd , TEST_WAIT, &event), "h_header", "HTTPD: header event (b)"         );
        is_strncmp(tap_ev_arg(event, "key"  ), "Another", SXE_LITERAL_LENGTH( "Another"), "HTTPD: header was 'Another'"     );
        is_strncmp(tap_ev_arg(event, "value"), "Foo"    , SXE_LITERAL_LENGTH( "Foo"    ), "HTTPD: header value was 'Foo'"   );
    }

    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_respond", "HTTPD: respond event");

    sxe_close(client);
    is_eq(test_tap_ev_queue_identifier_wait(q_httpd, TEST_WAIT, &event), "h_close", "HTTPD: close event");
}

int
main(void)
{
    SXE_HTTPD httpd;
    unsigned  id;

    plan_tests(43);

    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE=6");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTP=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_HTTPD=6"); /* comment this line to debug this test */

    test_sxe_register_and_init(1000);

    sxe_httpd_construct(&httpd, 2, 10, 512, 0);
    SXE_HTTPD_SET_HANDLER(&httpd, connect, h_connect);
    SXE_HTTPD_SET_HANDLER(&httpd, header,  h_header);
    SXE_HTTPD_SET_HANDLER(&httpd, respond, h_respond);
    SXE_HTTPD_SET_HANDLER(&httpd, close, h_close);
    ok((listener = test_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL, "HTTPD listening");

    for (id = 0; id < TEST_CASES; id++) {
        test_case(cases[id].tapname, cases[id].request, cases[id].header_count);
    }

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
