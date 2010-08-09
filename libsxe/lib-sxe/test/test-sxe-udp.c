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

static SXE *            connectee = NULL;
static SXE *            server;
static unsigned         read_state = 0;
static unsigned short   remote_port;         /* In host byte order */

static void
event_read(SXE * this, int length)
{
    SXEE62I("%s(length=%d)", __func__, length);

    remote_port = SXE_PEER_PORT(this);
    read_state++;

    switch (read_state) {
    case 1:
        ok(connectee                    ==           NULL,       "Not already connected"       );
        ok(length                       ==              5,       "5 bytes of data arrived"     );
        ok(        SXE_BUF_USED (this)  ==              5,       "5 bytes in receive buffer"   );
        ok(strncmp(SXE_BUF      (this), "HELO\n",       5) == 0, "'HELO\\n' arrived"           );
        connectee = this;
        break;

    default:
        diag("Unexpected read sequence number %u.\n", read_state);
    }

    SXER60("return");
}

static int
test_ev_loop_nonblock(void)
{
    SXEE61("%s()", __func__);
    ev_loop(ev_default_loop(EVFLAG_AUTO), EVLOOP_NONBLOCK);
    SXER60("return");
    return 1;
}

#define SXE_CAST_SSIZE_T_TO_LONG(_x) ((long)(_x))

static ssize_t SXE_STDCALL
test_recvfrom(SXE_SOCKET s, void *buf, SXE_SOCKET_SSIZE len, int flags, struct sockaddr *from, SXE_SOCKLEN_T *fromlen)
{
    SXE_UNUSED_PARAMETER(s);
    SXE_UNUSED_PARAMETER(buf);
    SXE_UNUSED_PARAMETER(len);
    SXE_UNUSED_PARAMETER(flags);
    SXE_UNUSED_PARAMETER(from);
    SXE_UNUSED_PARAMETER(fromlen);

    SXEE67("%s(s=%d,buf=%p,len=%ld,flags=%d,from=%p,fromlen=%p)", __func__, s, buf, SXE_CAST_SSIZE_T_TO_LONG(len), flags, from, fromlen);
    sxe_socket_set_last_error(SXE_SOCKET_ERROR(EINVAL));

    SXER60("return -1");
    return -1;
}
static int server_read_event_count = 0;

static void
test_server_event_read(SXE * this, int length)
{
    SXEE62I("%s(length=%d)", __func__, length);

    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(length);

    ++server_read_event_count;

    SXER60("return");
}

static int client_read_event_count = 0;

static void
test_client_event_read(SXE * this, int length)
{
    SXEE62I("%s(length=%d)", __func__, length);

    SXE_UNUSED_PARAMETER(this);
    SXE_UNUSED_PARAMETER(length);

    ++client_read_event_count;

    SXER60("return");
}

static void
test_case_setup_server_sxe_and_client_socket(int port, SXE ** sxe_server, int * client_socket, struct sockaddr_in * client_addr, struct sockaddr_in * to)
{
    SXEE61("%s()", __func__);

    sxe_register(1, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    *sxe_server = sxe_new_udp(NULL, "127.0.0.1", port, event_read);
    ok(sxe_listen(*sxe_server) == SXE_RETURN_OK, "listen on UDP server succeeded");

    ok((*client_socket = socket(AF_INET, SOCK_DGRAM, 0)) >= 0, "Created test client UDP socket");
    //diag("client socket is fd=%d", *client_socket);
    ok(sxe_socket_set_nonblock(*client_socket, 1) >= 0, "Made test client socket non-blocking");

    memset(client_addr, 0x00, sizeof(*client_addr));
    client_addr->sin_family      = AF_INET;
    client_addr->sin_port        = htons(0);
    client_addr->sin_addr.s_addr = htonl(INADDR_ANY);
    ok(bind(*client_socket, (struct sockaddr *)client_addr, sizeof(*client_addr)) >= 0, "Bound a random port to client UDP socket");

    memset(to, 0x00, sizeof(*to));
    to->sin_family         = AF_INET;
    to->sin_port           = htons(port);
    to->sin_addr.s_addr    = inet_addr("127.0.0.1");

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
    SXE * client;
    struct sockaddr_in addr;

    SXEE61("%s()", __func__);

    sxe_register(2, 0);
    ok(sxe_init() == SXE_RETURN_OK, "init succeeded");

    server_read_event_count = 0;
    client_read_event_count = 0;

    server = sxe_new_udp(NULL, "127.0.0.1", 0, test_server_event_read);
    ok(sxe_listen(server) == SXE_RETURN_OK, "listen on UDP server succeeded");

    client = sxe_new_udp(NULL, SXE_IP_ADDR_ANY, 0, test_client_event_read);
    ok(sxe_listen(client) == SXE_RETURN_OK, "listen on UDP client succeeded");

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    addr.sin_port = server->local_addr.sin_port;

    SXEL61("Using port %hu", addr.sin_port);

    is(sxe_write_to(client, "HELo\n", 5, &addr), SXE_RETURN_OK, "Sent query to server");
    test_ev_loop_wait(2);
    test_ev_loop_nonblock();

    is(server_read_event_count, 1, "Received event for server read");
    is(client_read_event_count, 0, "No events received on client");

    sxe_close(client);
    sxe_close(server);

    sxe_fini();

    SXER60("return");
}

static void
test_case_sxe_udp_happy_path(void)
{
    SXE_RETURN         result;
    int                port = 9053;
    ssize_t            sent;
    int                client_socket = -1;
    struct sockaddr_in client_addr;
    struct sockaddr_in to;
    char               buf[512];
    struct sockaddr_in source_addr;
    SXE_SOCKLEN_T      source_addr_len = sizeof(source_addr);

    SXEE61("%s()", __func__);

    test_case_setup_server_sxe_and_client_socket(port, &server, &client_socket, &client_addr, &to);

    is((sent = sendto(client_socket, "HELO\n", 5, 0, (struct sockaddr *)&to, sizeof(to))), 5, "Sent 5 bytes to server");

    if (sent < 0) {
        diag("Error: %s", sxe_socket_get_last_error_as_str());
    }

    test_ev_loop_nonblock();
    is(read_state, 1, "Received the first UDP packet");
    client_addr.sin_family      = AF_INET;
    client_addr.sin_port        = htons(remote_port);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    is((result = sxe_write_to(server, "ELHO\n", 5, &client_addr)), SXE_RETURN_OK, "Sent reply to client port on loopback address");

    is(recvfrom(client_socket, buf, sizeof(buf), 0, (struct sockaddr *)&source_addr, &source_addr_len), 5,
       "Received expected size reply from server");
    buf[5] = '\0';
    is_eq(buf, "ELHO\n", "Received expected reply from server");

    test_case_cleanup_server_sxe_and_client_socket(server, client_socket);

    SXER60("return");
}

static void
test_case_sxe_recvfrom_sendto_errors(void)
{
    SXE_RETURN         result;
    int                port = 9153;
    ssize_t            sent;
    int                client_socket = -1;
    struct sockaddr_in client_addr;
    struct sockaddr_in to;

    SXEE61("%s()", __func__);

    test_case_setup_server_sxe_and_client_socket(port, &server, &client_socket, &client_addr, &to);

    /* Fake an EINVAL return from recvfrom.
     */
    MOCK_SKIP_START(1);
    is((sent = sendto(client_socket, "HELO\n", 5, 0, (struct sockaddr *)&to, sizeof(to))), 5, "Sent 5 bytes to server: (%d) %s",
        sxe_socket_get_last_error(), sxe_socket_get_last_error_as_str());
    MOCK_SKIP_END;
    MOCK_SET_HOOK(recvfrom, test_recvfrom);
    test_ev_loop_nonblock();
    MOCK_SET_HOOK(recvfrom, recvfrom);
    is(read_state, 1, "Did not receive another UDP packet");

    /* Test sendto failure handling.
     */
    close(server->socket_as_fd);
    server->socket_as_fd = -1;
    is((result = sxe_write_to(server, "ELHO\n", 5, &client_addr)), SXE_RETURN_ERROR_INTERNAL,
       "Write to closed local/loopback client port fails");

    /* Over allocate SXE objects.
     */
    /* is(sxe_new_udp(NULL, "127.0.0.1", port, event_read), NULL, "Unable to allocate a third SXE object"); */

    test_case_cleanup_server_sxe_and_client_socket(server, client_socket);

    SXER60("return");
}

int
main(int argc, char *argv[]) {
    plan_tests(28);
    SXE_UNUSED_PARAMETER(argc);
    SXE_UNUSED_PARAMETER(argv);

    /* TODO: investigate failure on windows if udp_both_ends is run after happy_path test
     *   - it seems ev has an issue re-using a previously closed fd */
    test_case_sxe_udp_both_ends();
    test_case_sxe_udp_happy_path();
    test_case_sxe_recvfrom_sendto_errors();

    return exit_status();
}
