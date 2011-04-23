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
#include "mock.h"
#include "sxe-pool-tcp.h"
#include "sxe-spawn.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#ifdef WINDOWS_NT
#else

#define TEST_COMMAND           "./command.pl"
#define TEST_POOL_SIZE         2
#define NO_TIMEOUT             0
#define RESPONSE_SECONDS       2
#define INITIALIZATION_SECONDS 30

static void         * caller_info = NULL;
static const char   * write_data  = NULL;
static struct timeval current_timeval;

static void
test_event_log_line(SXE_LOG_LEVEL level, const char * line)
{
    unsigned length = strlen(line) - 1;

    SXE_UNUSED_ARGUMENT(level);

    /* Nota bene: You must NOT call anything that logs from this function! */

    /* Catch "TCP pool %s: Connection %u has failed %u times: giving up" messages in the log and send them to the test program.
     */
    if ((sxe_strnstr(&line[38], "TCP pool", length) != NULL) && (sxe_strnstr(&line[38], "times: giving up", length) != NULL)) {
        tap_ev_push(__func__, 1, "line", strdup(&line[39]));
        return;
    }

    fprintf(stderr, "%s\n", line);
}

static void
test_event_timeout(SXE_POOL_TCP * pool, void * info)
{
    SXEE83("%s(pool=%s, info=%p)", __func__, SXE_POOL_TCP_GET_NAME(pool), info);
    SXE_UNUSED_ARGUMENT(pool);
    SXE_UNUSED_ARGUMENT(info);
    tap_ev_push(__func__, 0);
    SXER80("return");
}

static void
test_event_ready_to_write(SXE_POOL_TCP * pool, void * info)
{
    SXE_UNUSED_ARGUMENT(pool);
    SXE_UNUSED_ARGUMENT(info);
    SXEE82("test_event_ready_to_write(pool=%s, info=%p)", SXE_POOL_TCP_GET_NAME(pool), info);
    tap_ev_push(__func__, 0);
    sxe_pool_tcp_write(pool, write_data, strlen(write_data), NULL);
    SXER80("return");
}

static void
test_event_connected(SXE* this)
{
    SXEE80I("test_event_connected()");
    tap_ev_push("test_event_connected", 1, "pid", SXE_USER_DATA_AS_INT(this));
    SXER80I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE81I("test_event_read(length=%d)", length);
    SXEA10(length != 0, "Read event has length == 0");
    tap_ev_push(__func__, 3, "this", this, "length", length, "buf", tap_dup(SXE_BUF(this), length));
    SXE_BUF_CLEAR(this);
    SXER80I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE80I("test_event_close()");
    tap_ev_push(__func__, 1, "this", this);
    SXER80I("return");
}

#ifdef __APPLE__
#define TYPE_TZ void *
#else
#define TYPE_TZ struct timezone *
#endif

static int
test_mock_gettimeofday(struct timeval * tv, TYPE_TZ tz)
{
    SXE_UNUSED_ARGUMENT(tz);
    *tv = current_timeval;
    return 0;
}

static void
test_case_no_initialization(void)
{
    SXE_POOL_TCP * pool;
    tap_ev         event;

    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "echopool", "perl", TEST_COMMAND, NULL, test_event_ready_to_write, NULL,
                                  test_event_read, NULL, NO_TIMEOUT, RESPONSE_SECONDS, test_event_timeout, caller_info);
    ok(pool != NULL,                                                                     "echopool was initialized");
    test_process_all_libev_events();

    /* Simulate normal operation.
     */
    write_data = "Bananas\n";
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)),     "test_event_ready_to_write", "Got 1st event: ready to write");

    /* The next two events can come in any order; this race condition seems unpreventable
     */
    event = test_tap_ev_shift_wait(5);

    if (strcmp(tap_ev_identifier(event),                    "test_event_ready_to_write") == 0) {
        pass(                                                                            "Got 2nd event: ready to write");
        is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_read",           "Got 3rd event: read");
    }
    else {
        is_eq(tap_ev_identifier(event),                     "test_event_read",           "Got 2nd event: read");
        is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_ready_to_write", "Got 3rd event: ready to write");
    }

    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)),     "test_event_read",           "Got 4th event: read");

    /* Simulate a hang.
     */
    write_data = "sleep 10\n";
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)),     "test_event_ready_to_write", "Got 5th event: ready to write");
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)),     "test_event_ready_to_write", "Got 6th event: ready to write");
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    sxe_pool_tcp_unqueue_ready_to_write_event(pool);

    /* Simulate a timeout.
     */
    SXEA11(gettimeofday(&current_timeval, NULL) == 0, "Error %s calling gettimeofday()", strerror(errno));
    current_timeval.tv_sec += RESPONSE_SECONDS + 2;
    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);
    sxe_pool_check_timeouts();
    MOCK_SET_HOOK(gettimeofday, gettimeofday);
    is_eq(tap_ev_identifier(tap_ev_shift()),                "test_event_timeout",        "Got 7th event: timeout");
    is_eq(tap_ev_identifier(tap_ev_shift()),                "test_event_timeout",        "Got 8th event: timeout");
    is(tap_ev_length(),                                     0,                           "No more events");
}

static void
test_case_initialization_hangs(void)
{
    SXE_POOL_TCP * pool;

    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "deadpool", "perl", TEST_COMMAND, NULL, test_event_ready_to_write,
                                  test_event_connected, test_event_read, NULL, INITIALIZATION_SECONDS,
                                  RESPONSE_SECONDS, test_event_timeout, caller_info);
    ok(pool != NULL,                                                            "deadpool was initialized");
    test_process_all_libev_events();

    /* Simulate an initialization timeout.
     */
    sxe_pool_tcp_queue_ready_to_write_event(pool);    /* Should not be called back */
    sxe_pool_tcp_queue_ready_to_write_event(pool);    /* Should not be called back */
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_connected", "1st program connected");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_connected", "2nd program connected");

    SXEA11(gettimeofday(&current_timeval, NULL) == 0, "Error %s calling gettimeofday()", strerror(errno));
    current_timeval.tv_sec += INITIALIZATION_SECONDS + 2;
    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);
    sxe_pool_check_timeouts();
    MOCK_SET_HOOK(gettimeofday, gettimeofday);
    is_eq(tap_ev_identifier(tap_ev_shift()),             "test_event_timeout",  "1st initialization timeout");
    is_eq(tap_ev_identifier(tap_ev_shift()),             "test_event_timeout",  "2nd initialization timeout");
    is(tap_ev_length(),                                  0,                     "No events other than initialization timeout");
    sxe_pool_tcp_delete(NULL, pool);    /* Prevent further events on this pool */
}

static void
test_case_initialization_succeeds(void)
{
    SXE_POOL_TCP * pool;
    tap_ev         event;
    SXE *          this;

    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "suckpool", "perl", TEST_COMMAND, "-n", test_event_ready_to_write,
                                  NULL, test_event_read, NULL, INITIALIZATION_SECONDS,
                                  RESPONSE_SECONDS, test_event_timeout, caller_info);
    ok(pool != NULL,                                                                 "suckpool was initialized");
    test_process_all_libev_events();

    /* Make sure two processes got started and sent us newlines.
     */
    write_data = "echo\n";
    sxe_pool_tcp_queue_ready_to_write_event(pool);    /* Should not be called back */
    event = test_tap_ev_shift_wait(5);
    this  = SXE_CAST_NOCONST(SXE *, tap_ev_arg(event, "this"));
    is_eq(tap_ev_identifier(event), "test_event_read",                               "1st program sent data");
    is_strncmp(tap_ev_arg(event, "buf"), "\n", tap_ev_arg(event, "length"),          "1st program sent a newline");
    event = test_tap_ev_shift_wait(5);
    is_eq(tap_ev_identifier(event), "test_event_read",                               "2nd program sent data");
    is_strncmp(tap_ev_arg(event, "buf"), "\n", tap_ev_arg(event, "length"),          "2nd program sent a newline");

    /* Mark both processes as initialized and make sure we can perform a transaction.
     */
    sxe_pool_tcp_initialized(this);
    this  = SXE_CAST_NOCONST(SXE *, tap_ev_arg(event, "this"));
    sxe_pool_tcp_initialized(this);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_ready_to_write", "1st program is ready to write to");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_read",           "1st program replied with echo");

    /* Now simulate the initialization time passing and make sure that we don't get any timeouts.
     */
    SXEA11(gettimeofday(&current_timeval, NULL) == 0, "Error %s calling gettimeofday()", strerror(errno));
    current_timeval.tv_sec += INITIALIZATION_SECONDS + 2;
    MOCK_SET_HOOK(gettimeofday, test_mock_gettimeofday);
    sxe_pool_check_timeouts();
    MOCK_SET_HOOK(gettimeofday, gettimeofday);
    is(tap_ev_length(),                                 0,                           "No other events");
}

static void
test_case_retries(void)
{
    SXE_POOL_TCP * pool;
    unsigned       i;

    SXEE61("%s()", __func__);
    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "retrypool", "sh", "-c", "perl -e 'sleep 0.1; exit 1;'", test_event_ready_to_write,
                                  NULL, test_event_read, test_event_close, NO_TIMEOUT, NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool != NULL,                                                                 "retrypool was initialized");

    /* Ask whether we're allowed to write */
    sxe_pool_tcp_queue_ready_to_write_event(pool);

    /* Within 0.1 seconds, the spawned processes will exit
     */
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_ready_to_write", "Received ready_to_write works while we have apparently viable connections");

    for (i = 1; i <= 6; i++) {
        is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",      "Close %u", i);
    }

    /* Confirm that no further close events occur
     */
    is(test_tap_ev_length_nowait(),                     0,                           "No more events");

    /* No connections are open now, and we're not trying to create more.
     * Further attempts to write will now be queued, and will never be honoured.
     */
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is(test_tap_ev_length_nowait(),                     0,                           "No more events");
    SXER60("return");
}

static void
test_case_single_command_bounces(void)
{
    SXE_POOL_TCP * pool;
    tap_ev         event;

    SXEE61("%s()", __func__);
    unlink("command-lock");
    sxe_log_hook_line_out(test_event_log_line);    /* hook log output */

    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "bouncepool", "perl", TEST_COMMAND, "-L", test_event_ready_to_write,
                                  NULL, test_event_read, test_event_close, NO_TIMEOUT, NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool != NULL,                                                                 "bouncepool was initialized");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Bouncing Betty closed once");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Bouncing Betty closed twice");
    event = test_tap_ev_shift_wait(5);
    is_eq(tap_ev_identifier(event),                     "test_event_log_line",       "Log line from TCP pool %s",
          (const char *)tap_ev_arg(event, "line"));
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Bouncing Betty closed three times");
    is(test_tap_ev_length_nowait(),                     0,                           "No more events");

    /* Make sure we can still use the other process.
     */
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_ready_to_write", "Got ready to write event and wrote 'echo'");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_read",           "Got reply to echo");

    /* Send the abort command to cover exiting via a signal
     */
    write_data = "abort\n";
    sxe_pool_tcp_queue_ready_to_write_event(pool);
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_ready_to_write", "Got ready to write event and wrote 'abort'");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Got 1st close event");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Got 2nd close event");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_log_line",       "Log line from TCP pool");
    is_eq(tap_ev_identifier(test_tap_ev_shift_wait(5)), "test_event_close",          "Got 3rd close event");
    SXER60("return");
}

static void
test_case_out_of_sxes(void)
{
    SXE *          hog_one;
    SXE_POOL_TCP * pool;

    SXEE61("%s()", __func__);
    SXEA10((hog_one = sxe_new_udp(NULL, "127.0.0.1", 0, test_event_read)) != NULL, "Failed to allocate a SXE to hog");
    pool = sxe_pool_tcp_new_spawn(TEST_POOL_SIZE, "failpool", "perl", TEST_COMMAND, NULL, test_event_ready_to_write,
                                  NULL, test_event_read, test_event_close, NO_TIMEOUT, NO_TIMEOUT, test_event_timeout, NULL);
    ok(pool != NULL,                                                                 "failpool was initialized");
    is(test_tap_ev_length_nowait(),                     0,                           "No events");
    SXER60("return");
}

static void
test_write_command_pl_file(void)
{
    FILE * command_pl_file;

    command_pl_file = fopen(TEST_COMMAND, "w+");
    SXEA11(command_pl_file != NULL, "ERROR - failed to open command.pl to create it in the build directory: %s", sxe_socket_get_last_error_as_str());

    fprintf(command_pl_file, "%s",
        "#!/usr/bin/perl                                                                                    \n"
        "                                                                                                   \n"
        "use strict;                                                                                        \n"
        "use warnings;                                                                                      \n"
        "use POSIX;                                                                                         \n"
        "                                                                                                   \n"
        "my $fd;                                                                                            \n"
        "                                                                                                   \n"
        "# Flush output on every line                                                                       \n"
        "#                                                                                                  \n"
        "$| = 1;                                                                                            \n"
        "                                                                                                   \n"
        "while (my $arg = shift(@ARGV)) {                                                                   \n"
        "    if ($arg eq '-n') {                                                                            \n"
        "        print qq[\\n];                                                                             \n"
        "    }                                                                                              \n"
        "    elsif ($arg eq '-L') {                                                                         \n"
        "        if (!($fd = POSIX::open('command-lock', &POSIX::O_CREAT | &POSIX::O_EXCL))) {              \n"
        "            print STDERR (qq[command.pl -L: Command is locked out\\n]);                            \n"
        "            exit(1);                                                                               \n"
        "        }                                                                                          \n"
        "    }                                                                                              \n"
        "}                                                                                                  \n"
        "                                                                                                   \n"
        "while(my $line = <>) {                                                                             \n"
        "    if ($line =~ /^abort/) {                                                                       \n"
        "        _exit(1);                                                                                  \n"
        "    }                                                                                              \n"
        "                                                                                                   \n"
        "    if ($line =~ /^sleep (\\d+)/) {                                                                \n"
        "        sleep($1);                                                                                 \n"
        "    }                                                                                              \n"
        "                                                                                                   \n"
        "    print $line;                                                                                   \n"
        "}                                                                                                  \n"
        "                                                                                                   \n"
        "if (defined($fd)) {                                                                                \n"
        "    close($fd);                                                                                    \n"
        "    unlink('command-lock');                                                                        \n"
        "}                                                                                                  \n"
    );

    fclose(command_pl_file);
}

#endif

int
main(void)
{
#ifdef WINDOWS_NT
    plan_tests(1);
    ok(1, "TODO: Disable spawn tests on Windows because this has yet to be implemented");
#else

    plan_tests(49);
    sxe_pool_tcp_register(3 * TEST_POOL_SIZE);
    SXEA10(sxe_init()                == SXE_RETURN_OK, "failed to initialize sxe");
    SXEA10(sxe_spawn_init()          == SXE_RETURN_OK, "failed to initialize sxe-spawn");
    test_write_command_pl_file();

    test_case_no_initialization();
    test_case_initialization_hangs();
    test_case_initialization_succeeds();
    test_case_retries();
    test_case_single_command_bounces();
    test_case_out_of_sxes();

#endif
    return exit_status();
}
