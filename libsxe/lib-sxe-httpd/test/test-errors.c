#include <string.h>

#include "sxe.h"
#include "sxe-httpd.h"
#include "sxe-util.h"
#include "sxe-test.h"
#include "tap.h"

#define TEST_WAIT 2
#define A100      "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
#define A1000     A100 A100 A100 A100 A100 A100 A100 A100 A100 A100
#define A16000    A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000 A1000

static SXE *listener;
static struct {
    const char *request;
    const char *response;
} cases[] = {
    { "GISSLERB\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET /\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "FIGZZ / HTTP/1.1\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET / HTTP/1.0\r\nSDF\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET / HTTP/1.0\r\nA B\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET / HTTP/1.0\r\nA:\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET / HTTP/1.0\r\n:B\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
    { "GET / HTTP/1.0\r\n A:B\r\n\r\n", "HTTP/1.1 400 Bad request\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 30\r\n\r\n<html>400 Bad request</html>\r\n" },
#ifndef WINDOWS_NT
    /* On windows, this causes a WSAECONNRESET event because the server close()s the connection
     * before reading all the data from the client. That's desired server behaviour, but causes
     * the test fail on Windows. Skip this case for now on Windows.
     */
    { "GET / HTTP/1.0\r\n"
      A16000 A1000
      ": really big\r\n\r\n",
        "HTTP/1.1 413 Request entity too large\r\nServer: sxe-httpd/1.0\r\nConnection: close\r\nContent-Type: text/html; charset=\"UTF-8\"\r\nContent-Length: 43\r\n\r\n<html>413 Request entity too large</html>\r\n" },
#endif
    { NULL, NULL }
};

static void
test_event_connect(SXE * this)
{
    SXEE61I("test_event_connect(id=%u)", SXE_USER_DATA_AS_INT(this));
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    SXEE63I("test_event_read(id=%u, length=%u): response length %u", SXE_USER_DATA_AS_INT(this), length, SXE_BUF_USED(this));
    tap_ev_push(__func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE61I("test_event_close(id=%u)", SXE_USER_DATA_AS_INT(this));
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

int
main(void)
{
    SXE_HTTPD    httpd;
    SXE        * client;
    tap_ev       event;
    const char * request;
    const char * response;
    unsigned     id = 0;

    plan_tests(5 * (sizeof(cases)/sizeof(cases[0]) - 1) + 1);

    sxe_register(1000, 0);
    SXEA10(sxe_init() == SXE_RETURN_OK, "Failed to initialize SXE package");

    sxe_httpd_construct(&httpd, 1, NULL, 0);
    ok((listener = sxe_httpd_listen(&httpd, "0.0.0.0", 0)) != NULL, "HTTPD listening");

    for (id = 0; cases[id].request != NULL; id = SXE_USER_DATA_AS_INT(client) + 1) {
        /* Send a request.
         */
        SXEA10((client = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connect, test_event_read, test_event_close)) != NULL,
               "Failed to allocate client SXE");
        SXE_USER_DATA_AS_INT(client) = id;
        SXEA10(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to connect to HTTPD");
        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_connect",         "Got a connected event");
        is(tap_ev_arg(event, "this"), client,                                               "It's the client");
        request = cases[SXE_USER_DATA_AS_INT(client)].request;
        SXEA10(sxe_write(client, request, strlen(request)) == SXE_RETURN_OK,                "Failed to write 1st request");

        /* Check the response.
         */
        for (;;) {
            SXEA10(strcmp(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_read") == 0, "No response from HTTPD");
            response = cases[id = SXE_USER_DATA_AS_INT((SXE *)(long)tap_ev_arg(event, "this"))].response;

            if ((unsigned)tap_ev_arg(event, "used") >= strlen(response)) {
                is(tap_ev_arg(event, "used"), strlen(response),                            "case %u: correct response length", 1 + id);
                is_strncmp(tap_ev_arg(event, "buf"), response, strlen(response),           "case %u: correct response", 1 + id);
                sxe_buf_clear((SXE *)(long)tap_ev_arg(event, "this"));
                break;
            }

            diag("received partial response");
        }

        is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &event), "test_event_close",           "Client is closed");
    }

    sxe_close(listener);
    return exit_status();
}

/* vim: set expandtab list listchars=tab\:^.,trail\:@ : */
