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

const char *cert = "../test/test-sxe-ssl.cert";
const char *pkey = "../test/test-sxe-ssl.pkey";

tap_ev_queue q_server;
tap_ev_queue q_client;

static void
test_event_connected(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER6I("return");
}

static unsigned test_event_read_consume = 0;

static void
test_event_read(SXE * this, int length)
{
    SXE_UNUSED_PARAMETER(length);
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("(length=%d)", length);
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
test_event_sent(SXE * this, SXE_RETURN final)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("()");
    tap_ev_queue_push(q, __func__, 2, "this", this, "result", final);
    SXER6I("return");
}

static void
test_event_buffer_consumed(void * user_data, SXE_BUFFER * buffer)
{
    SXE *        this = (SXE *)user_data;
    tap_ev_queue q    = SXE_USER_DATA(this);

    SXEE6I("(buffer=%p)", buffer);
    tap_ev_queue_push(q, __func__, 2, "this", this, "buffer", buffer);
    SXER6I("return");
}

static void
test_event_close(SXE * this)
{
    tap_ev_queue q = SXE_USER_DATA(this);
    SXEE6I("()");
    tap_ev_queue_push(q, __func__, 1, "this", this);
    SXER6I("return");
}

static void
test_case_chat(const char * from, SXE * s_from, tap_ev_queue q_from, const char * data, unsigned size,
               const char * to,   SXE * s_to,   tap_ev_queue q_to)
{
    SXE_RETURN   result;
    SXE_BUFFER * buffers;
    tap_ev       event;
    char       * readbuf;
    unsigned     i;
    unsigned     j;
    unsigned     n;

    SXEE6("(from=%s,s_from=%p,q_from=%p,data=%p,size=%u,to=%s,s_to=%p,q_to=%p)",
          from, s_from, q_from, data, size, to, s_to, q_to);
    n = (size + TEST_MAX_BUF - 1) / TEST_MAX_BUF;
    SXEA1((readbuf = calloc(TEST_COPIES, size)) != NULL, "Failed to allocate %u bytes", TEST_COPIES * size);
    SXEA1((buffers = malloc(n * TEST_COPIES * sizeof(SXE_BUFFER))) != NULL, "Failed to allocate %u buffers", n * TEST_COPIES);

    for (i = 0; i < TEST_COPIES; i++) {
        for (j = 0; j < n; j++) {
            if (i == TEST_COPIES - 1 && j == n - 1) {
                sxe_buffer_construct_plus(&buffers[i + TEST_COPIES * j], SXE_CAST_NOCONST(char *, &data[j * TEST_MAX_BUF]),
                                           j == n - 1 ? size - j * TEST_MAX_BUF : TEST_MAX_BUF,
                                           j == n - 1 ? size - j * TEST_MAX_BUF : TEST_MAX_BUF,
                                           s_from, test_event_buffer_consumed);
            }
            else {
                sxe_buffer_construct_const(&buffers[i + TEST_COPIES * j], &data[j * TEST_MAX_BUF],
                                           j == n - 1 ? size - j * TEST_MAX_BUF : TEST_MAX_BUF);
            }

            result = sxe_send_buffer(s_from, &buffers[i + TEST_COPIES * j]);
        }
    }

    event = test_tap_ev_queue_shift_wait(q_from, TEST_WAIT);
    is_eq(tap_ev_identifier(event), "test_event_buffer_consumed",                        "got %s buffer consumed event", from);
    is(SXE_CAST(SXE_RETURN, tap_ev_arg(event, "buffer")), &buffers[n * TEST_COPIES - 1], "got expected buffer");
    test_ev_queue_wait_read(q_to, TEST_WAIT, &event, s_to, "test_event_read", readbuf, TEST_COPIES * size, to);
    is_strncmp(data, readbuf, size,                                                      "%s read contents from %s", to, from);
    free(buffers);
    free(readbuf);
    SXER6("return");
}

static void
test_case_chat_from_client(SXE * client, SXE * server, const char * buffer, unsigned size)
{
    SXEE6("(client=%p,server=%p,buffer=%p,data=%u)", client, server, buffer, size);
    test_case_chat("client", client, q_client, buffer, size, "server", server, q_server);
    SXER6("return");
}

static void
test_case_chat_from_server(SXE * client, SXE * server, const char * buffer, unsigned size)
{
    test_case_chat("server", server, q_server, buffer, size, "client", client, q_client);
}

int
main(int argc, char *argv[])
{
    tap_ev         event;
    SXE          * listener = NULL;
    SXE          * server   = NULL;
    SXE          * client   = NULL;
    SXE_LOG_LEVEL  level    = sxe_log_get_level();

    SXE_UNUSED_PARAMETER(argc);

    q_server = tap_ev_queue_new();
    q_client = tap_ev_queue_new();

    tap_plan(156, TAP_FLAG_ON_FAILURE_EXIT, NULL);

    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_BUFFER=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LIST=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_POOL=5");
    putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE=5");         // Increase from level 5 if debugging this test
    sxe_register(6, 0);
    sxe_ssl_register(2);
    is(sxe_init(), SXE_RETURN_OK,                                                           "sxe_init succeeded");

    sxe_ssl_instrument_memory_functions();
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
        is(verified,   1,         "client verified server's certificate");

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
        is(verified,   0,         "server DID NOT verify clients's certificate");
    }

    /* Try to allocate another SSL connection; this will fail because we only
     * registered 2, and both are in use. */
    tap_set_test_case_name("SSL connections exhausted");
    {
        SXE * extra = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        sxe_listen(extra);
        is(sxe_ssl_accept(extra), SXE_RETURN_NO_UNUSED_ELEMENTS,                            "got SXE_RETURN_NO_UNUSED_ELEMENTS");
        is(sxe_ssl_connect(extra), SXE_RETURN_NO_UNUSED_ELEMENTS,                           "got SXE_RETURN_NO_UNUSED_ELEMENTS");
        sxe_close(extra);
    }

    tap_set_test_case_name("Simple conversation");
    SXEL5("test-sxe-ssl: idle right now");
    test_case_chat_from_client(client, server, "HELO", 4);
    SXEL5("test-sxe-ssl: idle right now");
    test_case_chat_from_server(client, server, "HITHERE", 7);
    SXEL5("test-sxe-ssl: idle right now");

    /* client -> server: something bigger than a single read, but only a
     * single write (16KB) */
    tap_set_test_case_name("Single write, multiple reads");
    {
        char s4000[] = S4000;

        test_case_chat_from_client(client, server, s4000, sizeof(s4000));
        SXEL5("test-sxe-ssl: idle right now");
        test_case_chat_from_server(client, server, "THANKS!", 7);
        SXEL5("test-sxe-ssl: idle right now");
    }

    /* client -> server: contents of a compiled program. */
    tap_set_test_case_name("Send test program file");
    {
        struct stat   sb;
        char        * data;
        int           fd = open(argv[0], O_RDONLY);
        int           ret;

        fstat(fd, &sb);
        data = malloc(sb.st_size);
        SXEA1((ret = read(fd, data, sb.st_size)) == sb.st_size, "Unexpected result from read: got %u bytes instead of %ld", ret,
              sb.st_size);

        SXEL5("test-sxe-ssl: idle right now");
        test_case_chat_from_client(client, server, data, sb.st_size);
        SXEL5("test-sxe-ssl: idle right now");
        test_case_chat_from_server(client, server, "THANKS!", 7);

        close(fd);
        free(data);
    }

    /* Now prove that consume and resume work on SSL sockets. This case proves
     * that the read callback is paused, but the internal data is actually
     * drained from the SSL buffer anyway. We can tell because the second read
     * event has the full SXE_BUF_USED() as its length, even though we
     * consumed.
     *
     * 1. We sxe_send() 1600 bytes.
     * 2. We'll only receive 1500 in the first read callback, because that's
     *    the SXE_BUF_SIZE.
     * 3. We consume 250 bytes. This pauses the read events, so we will not
     *    receive any more read callbacks; but we *will* continue to process
     *    read events "internally", so we'll read the remaining 100 bytes from
     *    the SSL read buffer into the SXE buffer.
     * 4. We resume the SXE using SXE_RESUME_IMMEDIATE (since there is no more
     *    data on the socket). We should get another read event with the
     *    remaining data in the buffer (length 1600 - 250).
     *
     * Furthermore, we want to prove that a paused SXE preserves the read
     * buffer until it is resumed. We'll do that by having the server reply
     * with (the same) 1600 bytes, and we'll arrange for the client *NOT* to
     * clear the SXE buffer on its callback. This should cause SXE not to
     * read any more SSL data until sxe_buf_clear() or sxe_buf_resume() are
     * called.
     */
    tap_set_test_case_name("first consume/resume");
    {
        SXE_RETURN result;
        char       data[1600]; /* garbage data -- not important */

        level = sxe_log_set_level(level);

        /* Put guards on the edges of the buffer */
        snprintf(data, sizeof(data), "ENSURE");
        snprintf(data + 250,  sizeof(data) - 250, "GUARD");
        snprintf(data + 1590, sizeof(data) - 1590, "FEDCBA9876");

        /* Note: this auto-clears after the first read event: see
         * test_event_read() */
        test_event_read_consume = 250;
        result = sxe_send(client, data, sizeof(data), test_event_sent);

        if (result == SXE_RETURN_IN_PROGRESS) {
            event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
            is_eq(tap_ev_identifier(event), "test_event_sent",                              "got client sent event");
            is(SXE_CAST(SXE_RETURN, tap_ev_arg(event, "result")), SXE_RETURN_OK,            "got successful send");
        }
        else {
            skip(2, "sent immediately - no need to wait for sent event");
        }

        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_read",                                  "got server read event");
        is(SXE_CAST(unsigned, tap_ev_arg(event, "used")), SXE_BUF_SIZE,                     "got %u bytes", SXE_BUF_SIZE);
        is_strncmp(tap_ev_arg(event, "buf"), "ENSURE", 6,                                   "got initial bytes");

        /* By now the data will already have been drained into the SXE,
         * because each call to queue_shift_wait() does a wait_for_events()
         * call, which calls ev_loop(). So now just fire a read event.
         */
        sxe_buf_resume(server, SXE_BUF_RESUME_IMMEDIATE);

        test_process_all_libev_events(); // Doesn't block
        event = tap_ev_queue_shift(q_server);
        is_eq(tap_ev_identifier(event), "test_event_read",                                  "got server read event");
        is(SXE_CAST(unsigned, tap_ev_arg(event, "used")), sizeof(data) - 250,               "got %u bytes", (unsigned)sizeof(data) - 250);
        is_strncmp(tap_ev_arg(event, "buf"), "GUARD", 5,                                    "got initial bytes");

        /* Now pause the client's read events, and reply with 1600 bytes from
         * the server. The client will have read all 1600 bytes from the
         * socket, since SSL reads in 16KB chunks; but the extra 100 bytes
         * should be sitting in memory somewhere waiting to be drained until
         * we ask for them. */
        sxe_pause(client);
        result = sxe_send(server, data, sizeof(data), test_event_sent);

        if (result == SXE_RETURN_IN_PROGRESS) {
            event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
            is_eq(tap_ev_identifier(event), "test_event_sent",                              "got server sent event");
            is(SXE_CAST(SXE_RETURN, tap_ev_arg(event, "result")), SXE_RETURN_OK,            "got successful send");
        }
        else {
            skip(2, "sent immediately - no need to wait for sent event");
        }

        test_process_all_libev_events();

        /* We should have loaded the first 1500 bytes into the client's
         * buffer; but the last 100 bytes should still be pending. */
        sxe_buf_resume(client, SXE_BUF_RESUME_IMMEDIATE);
        test_process_all_libev_events(); // Doesn't block

        event = tap_ev_queue_shift(q_client);
        is_eq(tap_ev_identifier(event), "test_event_read",                                  "got client read event");
        is(SXE_CAST(unsigned, tap_ev_arg(event, "used")), SXE_BUF_SIZE,                     "got %u bytes", SXE_BUF_SIZE);
        is_strncmp(tap_ev_arg(event, "buf"), "ENSURE", 6,                                   "got initial bytes");

        event = tap_ev_queue_shift(q_client);
        is_eq(tap_ev_identifier(event), "test_event_read",                                  "got client read event");
        is(SXE_CAST(unsigned, tap_ev_arg(event, "used")), sizeof(data) - SXE_BUF_SIZE,      "got %u bytes", (unsigned)sizeof(data) - SXE_BUF_SIZE);

        level = sxe_log_set_level(level);
    }

    SXEL5("test-sxe-ssl: beginning to close");

    /* Close the client, and ensure the server gets a close event. Note that
     * the client doesn't get a close event: SXE never generates close events
     * for explicit calls to sxe_close(), and SSL is no different. */
    {
        sxe_close(client);

        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
        is(tap_ev_arg(event, "this"), server,                                               "on the server");
    }

    SXEL5("test-sxe-ssl: finished first wave of SSL connections, and done closing");

    /* Now connect the client and server again, and this time chat for a while
     * without SSL, then turn on SSL late and prove things keep working. */
    tap_set_test_case_name("turn on SSL late");
    {
        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got client connected event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got server connected event");
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

        test_case_chat_from_client(client, server, "HELLO", 5);
        test_case_chat_from_server(client, server, "WORLD", 5);

        SXEL5("test-sxe-ssl: starting second wave of SSL connections");
        sxe_ssl_accept(server);
        sxe_ssl_connect(client);
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "client: SSL session established");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "server: SSL session established");

        SXEL5("test-sxe-ssl: idle right now");
        test_case_chat_from_client(client, server, "BRIGADIER", 9);
        SXEL5("test-sxe-ssl: idle right now");
        test_case_chat_from_server(client, server, "GENERAL", 7);
        SXEL5("test-sxe-ssl: idle right now");

        sxe_close(client);

        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
        is(tap_ev_arg(event, "this"), server,                                               "on the server");

        sxe_close(listener);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
    }

    SXEL5("test-sxe-ssl: finished second wave of SSL connections. Now trying to ruin SSL's day.");

    /* Now connect again, and this time, just close() the underlying fd of the
     * client socket. This causes an unexpected EOF in the SSL protocol, so
     * test it!
     */
    tap_set_test_case_name("unexpected EOF on client");
    {
        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        is(sxe_ssl_enable(listener), SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");
        is(sxe_ssl_enable(client),   SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got client connected event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got server connected event");
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

        test_case_chat_from_client(client, server, "HELLO", 5);
        test_case_chat_from_server(client, server, "WORLD", 5);

        /* Cause an unexpected EOF in the SSL stream! */
        SXEL5("test-sxe-ssl: closing socket %u", client->socket_as_fd);
        close(client->socket_as_fd);

        is(SXE_SEND_LITERAL(client, "BRIGADIER", test_event_sent), SXE_RETURN_ERROR_WRITE_FAILED, "got error sending on closed socket");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got client close event");
        is(tap_ev_arg(event, "this"), client,                                               "on the client");

        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
        is(tap_ev_arg(event, "this"), server,                                               "on the server");

        sxe_close(listener);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
    }

    SXEL5("test-sxe-ssl: still trying to ruin SSL's day.");

    /* This time just close() the underlying fd of the server socket. This
     * causes an unexpected EOF in the SSL protocol, so test it!
     */
    tap_set_test_case_name("unexpected EOF on server");
    {
        SXE_RETURN ret;

        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        is(sxe_ssl_enable(listener), SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");
        is(sxe_ssl_enable(client),   SXE_RETURN_OK,                                         "sxe_ssl_enable succeeded");
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                        "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK,  "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got client connected event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got server connected event");
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

        test_case_chat_from_client(client, server, "HELLO", 5);
        test_case_chat_from_server(client, server, "WORLD", 5);

        /* Cause an unexpected EOF in the SSL stream! */
        SXEL5("test-sxe-ssl: closing socket %u", server->socket_as_fd);
        close(server->socket_as_fd);

        is(SXE_SEND_LITERAL(client, "BRIGADIER", test_event_sent), SXE_RETURN_OK,           "got immediate send");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got client close event");
        is(tap_ev_arg(event, "this"), client,                                               "on the client");

        ret = SXE_SEND_LITERAL(server, "GENERAL", test_event_sent);
        ok(ret == SXE_RETURN_ERROR_WRITE_FAILED || ret == SXE_RETURN_ERROR_NO_CONNECTION,   "got write failure on closed socket");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
        is(tap_ev_arg(event, "this"), server,                                               "on the server");

        sxe_close(listener);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "got server close event");
    }

    /* This time just close() the underlying fd of the client socket, and then
     * try an sxe_close(). Because the socket is closed, the attempted
     * SSL_shutdown() will fail.
     */
    tap_set_test_case_name("Create an SSL pair, close(), then sxe_close()");
    {
        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        is(sxe_ssl_enable(listener), SXE_RETURN_OK,                                        "sxe_ssl_enable listener succeeded");
        is(sxe_ssl_enable(client),   SXE_RETURN_OK,                                        "sxe_ssl_enable client succeeded");
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                            "got client connected event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                            "got server connected event");
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

        test_case_chat_from_client(client, server, "HELLO", 5);
        test_case_chat_from_server(client, server, "WORLD", 5);

        /* Cause an unexpected EOF in the SSL stream! */
        SXEL5("test-sxe-ssl: closing socket %u", client->socket_as_fd);
        close(client->socket_as_fd);
        sxe_close(client);

        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                "got server close event");
        is(tap_ev_arg(event, "this"), server,                                              "on the server");

        sxe_close(listener);
        is_eq(tap_ev_identifier(event), "test_event_close",                                "got server close event");

        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                "got client close event");
    }

    /* Now connect the client and server again, and this time chat for a while
     * without SSL, then turn on SSL late and prove things keep working. */
    tap_set_test_case_name("break the SSL handshake by closing the fd");
    {
        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got client connected event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_connected",                             "got server connected event");
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));

        test_case_chat_from_client(client, server, "HELLO", 5);
        test_case_chat_from_server(client, server, "WORLD", 5);

        SXEL5("test-sxe-ssl: starting SSL handshakes (but they will be interrupted)");
        sxe_ssl_accept(server);
        sxe_ssl_connect(client);
        sxe_close(server);

        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                 "client: SSL session closed");
    }

    tap_set_test_case_name("SSL handshake with plain SXE");
    {
        listener = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        client   = sxe_new_tcp(NULL, "0.0.0.0", 0, test_event_connected, test_event_read, test_event_close);
        SXE_USER_DATA(listener) = q_server;
        SXE_USER_DATA(client)   = q_client;
        /* Only enabling SSL on the client, not the server listener */
        is(sxe_ssl_enable(client),   SXE_RETURN_OK,                                        "sxe_ssl_enable client succeeded");
        SXEA1(sxe_listen(listener) == SXE_RETURN_OK,                                       "Failed to sxe_listen()");
        SXEA1(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(listener)) == SXE_RETURN_OK, "Failed to sxe_connect()");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        server = SXE_CAST(SXE *, tap_ev_arg(event, "this"));
        is_eq(tap_ev_identifier(event), "test_event_connected",                            "got server connected event (just TCP, no SSL yet)");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_read",                                 "Server got some of the client SSL handshake");
        SXEA1(SXE_WRITE_LITERAL(server, "Decode this......to fail the SSL handshake") == SXE_RETURN_OK, "Error server failed to write");
        event = test_tap_ev_queue_shift_wait(q_client, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                "client: close event");
        event = test_tap_ev_queue_shift_wait(q_server, TEST_WAIT);
        is_eq(tap_ev_identifier(event), "test_event_close",                                "server: close event");
    }

    is(sxe_fini(), SXE_RETURN_OK, "finished with sxe");

    SXEL5("test-sxe-ssl: calling sxe_ssl_fini()");
    sxe_ssl_fini();
    SXEL5("test-sxe-ssl: all done!");

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
