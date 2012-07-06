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

#include <string.h>
#include <stdlib.h>
#include <unistd.h>   /* for getpid() on Linux */

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT        15
#define TEST_COPIES      10
#define TEST_MAX_BUF     30000

#define S25   "abcdefghijklmnopqrstuvwxy"
#define S10   "0987654321"
#define S5    "~!@#$"
#define S100  S25 S10 S5 S25 S10 S25
#define S1000 S100 S100 S100 S100 S100 S100 S100 S100 S100 S100
#define S4000 S1000 S1000 S1000 S1000

#ifndef SXE_DISABLE_OPENSSL

const char *cert = "../test/test-sxe-ssl-expired.cert";
const char *pkey = "../test/test-sxe-ssl-expired.pkey";

static void
test_event_connected(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("test_event_connected()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER6I("return");
}

static unsigned test_event_read_consume = 0;

static void
test_event_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("test_event_read(length=%d)", length);
    tap_ev_queue_push(q, __func__, 3, "this", this, "buf", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)), "used", SXE_BUF_USED(this));

    if (test_event_read_consume) {
        sxe_buf_consume(this, test_event_read_consume);
        test_event_read_consume = 0;
    }
    else {
        sxe_buf_clear(this);
    }

    SXER6I("return");
}

static void
test_event_close(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("test_event_close()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER6I("return");
}

int
main(void)
{
    tap_ev         event;
    SXE          * listener = NULL;
    SXE          * server   = NULL;
    SXE          * client   = NULL;
    tap_ev_queue   q_server = tap_ev_queue_new();
    tap_ev_queue   q_client = tap_ev_queue_new();

    plan_tests(22);

    sxe_register(6, 0);
    sxe_ssl_register(2);
    is(sxe_init(), SXE_RETURN_OK,                                                           "sxe_init succeeded");
    is(sxe_ssl_init(cert, pkey, cert, "."), SXE_RETURN_OK,                                  "sxe_ssl_init succeeded");

    listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
    client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
    SXE_USER_DATA(listener) = q_server;
    SXE_USER_DATA(client)   = q_client;

    /* Set up SSL on the listener and client. The listener doesn't actually
     * speak SSL, but any sockets it accepts will automatically inherit the
     * SSL flag, and will expect an SSL handshake to begin immediately. */
    is(sxe_ssl_enable(listener), SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");
    is(sxe_ssl_enable(client),   SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");

    SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
    SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");

    /* Ensure we get two connected events: one for the client and one for the
     * server. */
    event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
    is_eq(tap_ev_identifier(event), "test_event_connected",                             "got client connected event");
    event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
    is_eq(tap_ev_identifier(event), "test_event_connected",                             "got server connected event");
    server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

    /* Check that we can found out what protocol is being used to communicate. */
    tap_set_test_case_name("detect protocol");
    {
        int          verified = -1;
        int          bits     = -1;
        const char * cipher   = NULL;
        const char * version  = NULL;
        char         CN[64];
        char         issuer[64];
        char         shortbuf[8];

        sxe_ssl_get_info(client, &verified, &cipher, &bits, &version);
        is_eq(cipher,  "RC4-SHA", "client using cipher RC4-SHA");
        is_eq(version, "TLSv1",   "client using protocol TLSv1");
        is(bits,       128,       "client using 256 bits");
        is(verified,   0,         "client DID NOT verify server's certificate");

        is(sxe_ssl_get_peer_CN(client, CN, sizeof(CN)), SXE_LITERAL_LENGTH("*.sophoswdx.com"), "got correct length of peer CN");
        is_eq(CN, "*.sophoswdx.com", "correct peer name");

        is(sxe_ssl_get_peer_issuer(client, issuer, sizeof(issuer)), SXE_LITERAL_LENGTH("*.sophoswdx.com"), "got correct length of issuer");
        is_eq(issuer, "*.sophoswdx.com", "correct issuer name");

        is(sxe_ssl_get_peer_issuer(client, shortbuf, sizeof(shortbuf)), 7, "got correct length of truncated issuer");
        is_eq(shortbuf, "*.sopho", "correct issuer name");

        sxe_ssl_get_info(server, &verified, &cipher, &bits, &version);
        is_eq(cipher,  "RC4-SHA", "server using cipher RC4-SHA");
        is_eq(version, "TLSv1",   "server using protocol TLSv1");
        is(bits,       128,       "server using 256 bits");
        is(verified,   0,         "server DID NOT verify client's certificate");

        is(sxe_ssl_get_peer_CN(server, CN, sizeof(CN)), 0, "got 0-length peer CN");
        is(sxe_ssl_get_peer_issuer(server, issuer, sizeof(issuer)), 0, "got 0-length issuer");
    }

    return exit_status();
}

#else /* defined(SXE_DISABLE_OPENSSL) */

int
main(void)
{
    plan_skip_all("- skip this test program - no OpenSSL compiled into this libsxe");
    return exit_status();
}

#endif

/* vim: set expandtab list sw=4 sts=4 listchars=tab\:^.,trail\:@: */
