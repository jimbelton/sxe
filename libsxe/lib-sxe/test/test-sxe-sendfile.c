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

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "mock.h"
#include "sxe.h"
#include "sxe-log.h"
#include "sxe-socket.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT               5
#define TEST_SMALL_SEND_SIZE 512

#ifndef WINDOWS_NT
static char   * test_buf;
static unsigned test_buf_size   = 0;
static unsigned test_buf_length = 0;

static void
test_event_connected(SXE * this)
{
    SXEE60I("test_event_connected()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE63I("test_event_read(length=%d) // received %u of %u", length, test_buf_length + length, test_buf_size);
    SXEA12(test_buf_length + length <= test_buf_size, "Received %u bytes, but file has only %u", test_buf_length + length,
           test_buf_size);
    memcpy(&test_buf[test_buf_length], SXE_BUF(this), length);

    if ((test_buf_length < TEST_SMALL_SEND_SIZE) && (test_buf_length + length >= TEST_SMALL_SEND_SIZE)) {
        tap_ev_push(__func__, 1, "length", TEST_SMALL_SEND_SIZE);
    }
    else if (test_buf_length + length >= test_buf_size) {
        tap_ev_push(__func__, 1, "length", test_buf_size);
    }

    test_buf_length += length;
    SXE_BUF_CLEAR(this);
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE60I("test_event_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER60I("return");
}

static void
test_event_sendfile_complete(SXE * this, SXE_RETURN sxe_return)
{
    SXEE61I("test_event_sendfile_complete(sxe_return=%s)", sxe_return_to_string(sxe_return));
    tap_ev_push(__func__, 2, "this", this, "sxe_return", sxe_return);
    SXER60I("return");
}

#ifdef __APPLE__
static int
test_mock_sendfile(int in_fd, int out_fd, off_t offset, off_t *len, struct sf_hdtr *ign, int reserved)
{
    SXE_UNUSED_ARGUMENT(in_fd);
    SXE_UNUSED_ARGUMENT(out_fd);
    SXE_UNUSED_ARGUMENT(offset);
    SXE_UNUSED_ARGUMENT(len);
    SXE_UNUSED_ARGUMENT(ign);
    SXE_UNUSED_ARGUMENT(reserved);
    SXEE64("test_mock_sendfile(in_fd=%d, out_fd=%d, offset=%lu, len=%lu)", in_fd, out_fd, (unsigned long)offset, (unsigned long)*len);

    errno = EBADF;

    SXER60("return -1 // errno=EBADF");
    return -1;
}
#else /* !__APPLE__ */
static ssize_t
test_mock_sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
    SXE_UNUSED_ARGUMENT(out_fd);
    SXE_UNUSED_ARGUMENT(in_fd);
    SXE_UNUSED_ARGUMENT(offset);
    SXE_UNUSED_ARGUMENT(count);
    SXEE64("test_mock_sendfile(out_fd=%d, in_fd=%d, offset=%p, count=%lu)", out_fd, in_fd, offset, count);

    errno = EBADF;

    SXER60("return -1 // errno=EBADF");
    return -1;
}
#endif /* !__APPLE__ */
#endif /* !WINDOWS_NT */

int
main(void)
{
#ifdef WINDOWS_NT
    plan_skip_all("sxe_sendfile is not currently supported on Windows");    /* TODO: Add support using Windows TransmitFile() */
#else
    char        tempfile[PATH_MAX];
    int         in_fd;
    struct stat in_stat;
    SXE       * client;
    SXE       * server;
    socklen_t   size;
    int         tcp_send_buffer_size;
    tap_ev      ev;
    int         bytes_read;
    unsigned    bytes_compared;
    char        buffer[4096];
    off_t       offset;

    sxe_log_set_level(SXE_LOG_LEVEL_LIBRARY_TRACE);
    plan_tests(22);
    sxe_register(3, 0);

    is(sxe_init(), SXE_RETURN_OK, "SXE initialized");
    ok((server = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close)) != NULL,
        "Got new TCP SXE for server");
    is(sxe_listen(server), SXE_RETURN_OK, "SXE listening on server");
    ok((client = sxe_new_tcp(NULL, "INADDR_ANY", 0, test_event_connected, test_event_read, test_event_close)) != NULL,
        "Got new TCP SXE for client");
    is(sxe_connect(client, "127.0.0.1", SXE_LOCAL_PORT(server)), SXE_RETURN_OK, "SXE connecting from client");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, NULL), "test_event_connected", "SXE connected from client");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, NULL), "test_event_connected", "SXE connected from server");

    /* Write a file larger than the send buffer size */
    size = sizeof(tcp_send_buffer_size);
    SXEA13(getsockopt(client->socket, SOL_SOCKET, SO_SNDBUF, &tcp_send_buffer_size, &size) >= 0,
           "socket %d: couldn't get SO_SNDBUF: (%d) %s", client->socket, sxe_socket_get_last_error(),
           sxe_socket_get_last_error_as_str());

    {
        int fd, i;
        int len = snprintf(buffer, sizeof buffer,
                           "I am slowly going crazy, one two three four five six switch; "
                           "crazy going slowly am I, six five four three two one switch\n");
        snprintf(tempfile, sizeof tempfile, "./test-XXXXXX");
        SXEA11((fd = mkstemp(tempfile)) >= 0,                            "Failed to mkstemp '%s'", tempfile);
        for (i = 0; i < tcp_send_buffer_size * 4 + 1024; ) {
            i += write(fd, buffer, len);
        }

        lseek(fd, 0, SEEK_SET);
        in_fd = fd;
    }

    SXEA11(stat(tempfile, &in_stat) >= 0,                                "Failed to stat '%s'", tempfile);
    SXEA11((test_buf = malloc(test_buf_size = in_stat.st_size)) != NULL, "Failed to allocate %u byte buffer", test_buf_size);
    SXEL12("Created a tempfile %u bytes (send buffer %u)", in_stat.st_size, tcp_send_buffer_size);

    /* Test a small send */

    SXEA12(tcp_send_buffer_size > TEST_SMALL_SEND_SIZE, "TCP send buffer size %u is <= %u; make TEST_SMALL_SEND_SIZE smaller!",
           tcp_send_buffer_size, TEST_SMALL_SEND_SIZE);
    offset = 0;
    is(sxe_sendfile(client, in_fd, &offset, TEST_SMALL_SEND_SIZE, test_event_sendfile_complete), SXE_RETURN_OK,
       "sxe_sendfile() returns OK - %u bytes were written immediately", TEST_SMALL_SEND_SIZE);
    is_eq(tap_ev_identifier(ev = tap_ev_shift()), "test_event_sendfile_complete", "Called back before sxe_sendfile() completed");
    is(tap_ev_arg(ev, "sxe_return"), SXE_RETURN_OK, "Sendfile operation was successful");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_read", "SXE server got read event");
    is(tap_ev_arg(ev, "length"), TEST_SMALL_SEND_SIZE, "Server got first %u bytes", TEST_SMALL_SEND_SIZE);

    /* Test a big send; size required to overun the output buffer was determined experimentally */

    SXEA12(test_buf_size > (unsigned)tcp_send_buffer_size * 3 + 1024, "File size %u is <= %u; use a bigger file!",
           test_buf_size,  (unsigned)tcp_send_buffer_size * 3 + 1024);
    is(sxe_sendfile(client, in_fd, &offset, tcp_send_buffer_size * 3 + 1024, test_event_sendfile_complete), SXE_RETURN_IN_PROGRESS,
       "sxe_sendfile() returns IN_PROGRESS when tcp buffer has less room that what needs to be written");
    is(tap_ev_length(), 0, "Callback is not called when sxe_sendfile() is incomplete");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, &ev), "test_event_sendfile_complete", "SXE sendfile completed asynchronously");
    is(tap_ev_arg(ev, "sxe_return"), SXE_RETURN_OK, "Sendfile operation was successful");
    is(tap_ev_length(), 0, "Server has not yet received the entire file");

    /* Test sending the rest of the file */

    is(sxe_sendfile(client, in_fd, &offset, test_buf_size, test_event_sendfile_complete), SXE_RETURN_IN_PROGRESS,
       "sxe_sendfile() returns IN_PROGRESS when not all bytes are written");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, NULL), "test_event_sendfile_complete", "SXE sendfile completed asynchronously");
    is_eq(test_tap_ev_identifier_wait(TEST_WAIT, NULL), "test_event_read",              "Server got the rest of the file");
    diag("received %u of %u", test_buf_length, test_buf_size);
    close(in_fd);

    SXEA11((in_fd = open(tempfile, O_RDONLY)) >= 0, "Failed to reopen '%s'", tempfile);

    for (bytes_compared = 0; (bytes_read = read(in_fd, buffer, sizeof(buffer))) > 0; bytes_compared += bytes_read) {
        SXEA12(bytes_compared + bytes_read <= test_buf_size, "Read more (%u) from file than last time (%u)",
               bytes_compared + bytes_read, test_buf_size);
        if (memcmp(buffer, &test_buf[bytes_compared], bytes_read)) {
            SXEL10("=== received:");
            SXED10(buffer, bytes_read);
            SXEL10("=== expected:");
            SXED10(&test_buf[bytes_compared], bytes_read);
        }
        SXEA12(memcmp(buffer, &test_buf[bytes_compared], bytes_read) == 0, "Mismatch in sent file in bytes %u .. %u",
               bytes_compared, bytes_compared + bytes_read - 1);
    }

    close(in_fd);
    is(bytes_read, 0, "Entire file was correctly received");

    /* Test the sendfail failure case.
     */
    SXEA11((in_fd = open(tempfile, O_RDONLY)) >= 0, "Failed to open '%s' for the 3rd time", tempfile);
    MOCK_SET_HOOK(sendfile, test_mock_sendfile);
    offset = 0;
    is(sxe_sendfile(client, in_fd, &offset, test_buf_size, test_event_sendfile_complete), SXE_RETURN_ERROR_WRITE_FAILED,
       "sxe_sendfile() returns ERROR_WRITE_FAILED when sendfile fails");

    unlink(tempfile);
#endif

    return exit_status();
}
