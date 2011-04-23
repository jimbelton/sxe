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
#include <unistd.h> /* for __func__ on Windows */

#include "mock.h"
#include "sxe.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT 5.0

static void
test_event_read(SXE * this, int length)
{
    SXEE62I("%s(length=%d)", __func__, length);
    tap_ev_push(__func__, 4, "this",   this,
                             "length", length,
                             "buf",    tap_dup(SXE_BUF(this), SXE_BUF_USED(this)),
                             "used",   SXE_BUF_USED(this));
    SXER60("return");
}

#define SXE_CAST_SSIZE_T_TO_LONG(_x) ((long)(_x))

static MOCK_SSIZE_T SXE_STDCALL
test_mock_recvfrom(MOCK_SOCKET s, MOCK_SOCKET_VOID *buf, MOCK_SOCKET_SSIZE_T len, int flags, struct sockaddr *from, MOCK_SOCKLEN_T *fromlen)
{
    SXE_UNUSED_PARAMETER(s);
    SXE_UNUSED_PARAMETER(buf);
    SXE_UNUSED_PARAMETER(len);
    SXE_UNUSED_PARAMETER(flags);
    SXE_UNUSED_PARAMETER(from);
    SXE_UNUSED_PARAMETER(fromlen);

    SXEE67("%s(s=%d,buf=%p,len=%ld,flags=%d,from=%p,fromlen=%p)", __func__, s, buf, SXE_CAST_SSIZE_T_TO_LONG(len), flags, from, fromlen);
    tap_ev_push(__func__, 0);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EINVAL));
    SXER60("return -1");
    return -1;
}

static void
test_case_setup_server_sxe_and_client_socket(int port, SXE ** sxe_server, int * client_socket, struct sockaddr_in * client_addr, struct sockaddr_in * server_address)
{
    SXEE61("%s()", __func__);

    sxe_register(1, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    *sxe_server = sxe_new_udp(NULL, "127.0.0.1", port, test_event_read);
    ok(sxe_listen(*sxe_server) == SXE_RETURN_OK, "listen on UDP server succeeded");

    ok((*client_socket = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, "Created test client UDP socket");
    //diag("client socket is fd=%d", *client_socket);
    ok(sxe_socket_set_nonblock(*client_socket, 1) >= 0, "Made test client socket non-blocking");

    memset(client_addr, 0x00, sizeof(*client_addr));
    client_addr->sin_family      = AF_INET;
    client_addr->sin_port        = htons(0);
    client_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    ok(bind(*client_socket, (struct sockaddr *)client_addr, sizeof(*client_addr)) >= 0, "Bound a random port to client UDP socket");

    memset(server_address, 0x00, sizeof(*server_address));
    server_address->sin_family         = AF_INET;
    server_address->sin_port           = htons(port);
    server_address->sin_addr.s_addr    = inet_addr("127.0.0.1");

    SXER60("return");
}

static void
test_case_cleanup_server_sxe_and_client_socket(SXE * sxe_server, int client_socket)
{
    SXEE61("%s()", __func__);

    sxe_close(sxe_server);

    if (client_socket > 0) {
        CLOSESOCKET(client_socket);
    }

    sxe_fini();

    SXER60("return");
}

static void
test_case_sxe_udp_both_ends(void)
{
    SXE              * server;
    SXE              * client;
    struct sockaddr_in addr;
    tap_ev             ev;

    SXEE61("%s()", __func__);

    sxe_register(2, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    server = sxe_new_udp(NULL, "127.0.0.1", 0, test_event_read);
    ok(sxe_listen(server) == SXE_RETURN_OK, "listen on UDP server succeeded");

    client = sxe_new_udp(NULL, SXE_IP_ADDR_ANY, 0, test_event_read);
    ok(sxe_listen(client) == SXE_RETURN_OK, "listen on UDP client succeeded");

    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port        = SXE_LOCAL_PORT(server);
    addr.sin_port        = htons(addr.sin_port);      /* Must be a separate step: htons can't wrap ntohs */
    is(sxe_write_to(client, "HELo\n", 5, &addr), SXE_RETURN_OK,           "Sent query to server (port %hu)", SXE_LOCAL_PORT(server));
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_read", "Got a read event");
    is(tap_ev_arg(ev, "this"),   server,                                  "...on the server");
    is(tap_ev_arg(ev, "length"), SXE_LITERAL_LENGTH("HELo\n"),            "'length' is %u", (unsigned)SXE_LITERAL_LENGTH("HELo\n"));
    is(SXE_CAST_NOCONST(unsigned, tap_ev_arg(ev, "used")),
                                 SXE_LITERAL_LENGTH("HELo\n"),            "'used' is %u",   (unsigned)SXE_LITERAL_LENGTH("HELo\n"));
    is_eq(tap_ev_arg(ev, "buf"), "HELo\n",                                "'buf' is 'HELo\\n'");
    is(tap_ev_length(), 0,                                                "No more events in the queue");

    sxe_close(client);
    sxe_close(server);
    sxe_fini();

    SXER60("return");
}

static void
test_case_sxe_udp_happy_path(void)
{
    SXE              * server;
    SXE_RETURN         result;
    int                port = 9053;
    ssize_t            sent;
    int                client_socket = -1;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_address;
    char               buf[512];
    struct sockaddr_in source_addr;
    SXE_SOCKLEN_T      source_addr_len = sizeof(source_addr);
    tap_ev             ev;
    int                rc;
    int                last_socket_error;
    int                read_ok = 0;

    SXEE61("%s()", __func__);

    test_case_setup_server_sxe_and_client_socket(port, &server, &client_socket, &client_addr, &server_address);

    is((sent = sendto(client_socket, "HELO\n", 5, 0, (struct sockaddr *)&server_address, sizeof(server_address))), 5, "Sent 5 bytes to server");

    if (sent < 0) {
        diag("Error: %s", sxe_socket_get_last_error_as_str());
    }

    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_read", "Got a read event");
    is(tap_ev_arg(ev, "this"),   server,                                  "...on the server");

    client_addr.sin_family      = AF_INET;
    client_addr.sin_port        = SXE_PEER_PORT(server);
    client_addr.sin_port        = htons(client_addr.sin_port);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");       /* Must be a separate step: htons can't wrap ntohs */
    is((result = sxe_write_to(server, "ELHO\n", 5, &client_addr)), SXE_RETURN_OK, "Sent reply to client port on loopback address");

READ_AGAIN:
    rc = recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr *)&source_addr, &source_addr_len);
    if (rc == 5) {
        read_ok = 1;
    }
    else {
        last_socket_error = sxe_socket_get_last_error();
        if (last_socket_error == SXE_SOCKET_ERROR(EWOULDBLOCK)) { /* EAGAIN */
            SXEL60("Got an EAGAIN error, will try to read again...");
            usleep(100000); /* 100 ms */
            goto READ_AGAIN;
        }
    }

    ok(read_ok, "Received expected size reply from server");
    buf[5] = '\0';
    is_eq(buf, "ELHO\n", "Received expected reply from server");

    test_case_cleanup_server_sxe_and_client_socket(server, client_socket);

    SXER60("return");
}

static void
test_case_sxe_recvfrom_sendto_errors(void)
{
    SXE              * server;
    SXE_RETURN         result;
    int                port = 9153;
    ssize_t            sent;
    int                client_socket = -1;
    struct sockaddr_in client_addr;
    struct sockaddr_in server_address;

    SXEE61("%s()", __func__);

    test_case_setup_server_sxe_and_client_socket(port, &server, &client_socket, &client_addr, &server_address);

    /* Fake an EINVAL return from recvfrom.
     */
    is((sent = sendto(client_socket, "HELO\n", 5, 0, (struct sockaddr *)&server_address, sizeof(server_address))), 5,
       "Sent 5 bytes to server: (%d) %s", sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
    MOCK_SET_HOOK(recvfrom, test_mock_recvfrom);
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, NULL), "test_mock_recvfrom", "Mocked recvfrom called by server");
    MOCK_SET_HOOK(recvfrom, recvfrom);

    /* Test sendto failure handling.
     */
    close(server->socket_as_fd);
    server->socket_as_fd = -1;
    is((result = sxe_write_to(server, "ELHO\n", 5, &client_addr)), SXE_RETURN_ERROR_INTERNAL,
       "Write to closed local/loopback client port fails");

    /* Over allocate SXE objects.
     */
    /* is(sxe_new_udp(NULL, "127.0.0.1", port, test_event_read), NULL, "Unable to allocate a third SXE object"); */

    test_case_cleanup_server_sxe_and_client_socket(server, client_socket);

    SXER60("return");
}

int
main(void)
{
    plan_tests(29);
    test_case_sxe_recvfrom_sendto_errors();    /* Don't do last, to catch any spurious events that this might generate - yuck */
    test_case_sxe_udp_happy_path();
    test_case_sxe_udp_both_ends();
    return exit_status();
}
