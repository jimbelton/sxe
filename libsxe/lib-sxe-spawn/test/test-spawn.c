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
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ev.h"
#include "sxe.h"
#include "sxe-spawn.h"
#include "sxe-test.h"
#include "sxe-util.h"
#include "tap.h"

#define TEST_WAIT 5

#ifdef WIN32

#define TEST_ON_WINDOWS 1

static void pause(void) {}

#else

#define TEST_ON_WINDOWS 0

static void
test_event_connected(SXE * this)
{
    SXEE60I("test_event_connected()");
    SXE_UNUSED_PARAMETER(this);
    tap_ev_push(__func__, 0);
    SXER60I("return");
}

static void
test_event_read(SXE * this, int length)
{
    SXEE61I("test_event_read(length=%d)", length);
    tap_ev_push(__func__, 2, "length", length, "buffer", tap_dup(SXE_BUF(this), SXE_BUF_USED(this)));
    SXER60I("return");
}

static void
test_event_close(SXE * this)
{
    SXEE80I("test_event_close()");
    SXE_UNUSED_PARAMETER(this);
    tap_ev_push(__func__, 0);
    SXER80I("return");
}

#endif

static int
test_program_reader(void)
{
    char buffer[1024];
    int  length;
    int  ret;

    if ((length = read(fileno(stdin), buffer, sizeof(buffer))) <= 0) {
        SXEL22("test_program_reader: read returned %d: %s", length, length == 0 ? "EOF" : strerror(errno));
        usleep(100000); /* sleep a tenth of a second if pipe broken */
        return 1;
    }

    SXEA13((ret = write(fileno(stdout), buffer, length)) == length, "Expected to write %d bytes, wrote %d: %s",
           length, ret, strerror(errno));

    pause();   /* Wait for SIGPIPE */
    return 0;
}

static int
test_program_writer(void)
{
    int  length;

    if ((length = write(fileno(stdout), "hello", sizeof("hello"))) < (int)sizeof("hello")) {
        SXEL23("test_program_writer: write returned %d, expected %d: %s", length, sizeof("hello"), strerror(errno));
        return 1;
    }

    pause();   /* Wait for SIGPIPE */
    return 0;
}

static int
test_program_close(void)
{
    char buffer[1024];
    int  length;
    int  ret;

    SXEA12((ret = write(fileno(stdout), "UP", 2)) == 2, "Expected to write 2 bytes, wrote %d: %s", ret, strerror(errno));

    if ((length = read(fileno(stdin), buffer, sizeof(buffer))) <= 0) {
        SXEL22("test_program_close: read returned %d: %s", length, length == 0 ? "EOF" : strerror(errno));
        return 1;
    }

    return 0;
}

int
main(int argc, char ** argv)
{
#ifdef WIN32
#else
    SXE_RETURN result;
    SXE_SPAWN  spawn;
    int        status;
    double     total_seconds_waited;
    tap_ev     event;
    pid_t      pid;
#endif

    if (argc > 1) {
        if (strcmp(argv[1], "-r") == 0) {
            return test_program_reader();
        }
        else if (strcmp(argv[1], "-w") == 0) {
            return test_program_writer();
        }
        else if (strcmp(argv[1], "-c") == 0) {
            return test_program_close();
        }

        fprintf(stderr, "usage: test/test-spawn.t [-r|-w|-c]\n");
        return 1;
    }

#ifdef WIN32
    plan_tests(2);
#else
    plan_tests(29);
#endif

    sxe_register(1, 0);   /* One SXE object to allow only a single simultaneous spawn in this test */
    is(sxe_init(),                          SXE_RETURN_OK,                    "Initialized: sxe");
    is(sxe_spawn_init(),                    SXE_RETURN_OK,                    "Initialized: sxe-spawn");

#ifdef WIN32
    /* todo: refactor tests once sxe_spawn() exists on Windows */
#else
    /* Tests with a non-existent command */
    result = sxe_spawn(NULL, &spawn, "non-existent-command", NULL, NULL, test_event_connected, test_event_read, test_event_close);

#ifdef __APPLE__
    is(result,                              SXE_RETURN_ERROR_COMMAND_NOT_RUN, "Failed to spawn non-existent-command");
    skip(3,                                                                   "Failed to spawn non-existent-command");
#else
    is(result,                              SXE_RETURN_OK,                    "'Succeeded' in spawning non-existent-command");

    result = sxe_spawn(NULL, &spawn, "non-existent-command", NULL, NULL, test_event_connected, test_event_read, test_event_close);
    ok(result !=                            SXE_RETURN_OK,                    "Failed to spawn a second non-existent-command");

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_connected",           "Got the first connected event");
    tap_ev_free(event);

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_close",               "Got the first close event");
    tap_ev_free(event);
#endif

    SXEL60("----------------------------------------");
    SXEL60("Make sure spawned program blocks in read");
    SXEL60("----------------------------------------");

    result = sxe_spawn(NULL, &spawn, argv[0], "-r", NULL, test_event_connected, test_event_read, test_event_close);
    is(result,                              SXE_RETURN_OK,                    "Succeeded in spawning %s -r", argv[0]);
    is((pid = SXE_SPAWN_GET_PID(&spawn)),   SXE_USER_DATA_AS_INT(spawn.sxe),  "Spawned comand's PID is stored in SXE user data");

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_connected",           "Got the second connected event");
    tap_ev_free(event);

    is(waitpid(pid, &status, WNOHANG),      0,                                "Process %d is alive", pid);
    kill(pid, SIGTERM);
    is(waitpid(pid, &status, 0),            pid,                              "Process %d has been killed", pid);

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_close",               "Got the second close event");
    tap_ev_free(event);

    SXEL60("-----------------------------------------");
    SXEL60("Make sure spawned program blocks in write");
    SXEL60("-----------------------------------------");

    result = sxe_spawn(NULL, &spawn, argv[0], "-w", NULL, NULL, test_event_read, test_event_close);
    is(result,                              SXE_RETURN_OK,                    "Succeeded in spawning %s -w", argv[0]);
    pid    = SXE_SPAWN_GET_PID(&spawn);

    event  = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_read",                "Received the first read event");
    is(   tap_ev_arg(event, "length"),      6,                                "It is 6 bytes long");
    is_eq(tap_ev_arg(event, "buffer"),      "hello",                          "It is \"hello\\0\"");
    tap_ev_free(event);

    is(waitpid(pid, &status, WNOHANG),      0,                                "Process %d is alive!", pid);
    is(sxe_spawn_kill(&spawn, SIGHUP),      SXE_RETURN_OK,                    "Successfully killed process %d (SIGHUP)", pid);
    is(waitpid(pid, &status, 0),            pid,                              "Process %d has exited", pid);
    is(sxe_spawn_kill(&spawn, SIGHUP),      SXE_RETURN_ERROR_INTERNAL,        "Failed to kill dead process %d (SIGHUP)", pid);

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_close",               "Got the third close event");
    tap_ev_free(event);

    SXEL60("------------------------------------------------------");
    SXEL60("Make sure spawned program terminates when fd is closed");
    SXEL60("------------------------------------------------------");

    result = sxe_spawn(NULL, &spawn, argv[0], "-c", NULL, test_event_connected, test_event_read, test_event_close);
    is(result,                              SXE_RETURN_OK,                    "Succeeded in spawning %s -c", argv[0]);
    pid    = SXE_SPAWN_GET_PID(&spawn);
    is(waitpid(pid, &status, WNOHANG),      0,                                "Process %d is alive!", pid);

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(tap_ev_identifier(event),         "test_event_connected",           "Got the third connected event");
    tap_ev_free(event);

    event = test_tap_ev_shift_wait(TEST_WAIT);
    is_eq(     tap_ev_identifier(event),    "test_event_read",                "Received a second read event");
    is(        tap_ev_arg(event, "length"), 2,                                "It is 2 bytes long");
    is_strncmp(tap_ev_arg(event, "buffer"), "UP", 2,                          "It is \"UP\"");
    tap_ev_free(event);

    SXEL60("Closing connection to spawned program");
    sxe_close(SXE_SPAWN_GET_SXE(&spawn));

    /* TODO - Add the code below to the tests above to check definitively that spawned children have exited */
    total_seconds_waited = 0;

    do {
        usleep(10000); /* sleep a hundredth of a second */
        total_seconds_waited += 0.01;
        waitpid(pid, &status, WNOHANG);
        SXEL62("Spawned child[%d]: has %s", pid, WIFEXITED(status) ? "exited" : "not exited yet");
    } while(!WIFEXITED(status) && (total_seconds_waited < 2.0));     /* don't wait for more than 2 seconds */

    SXEL61("Spawned child: total_seconds_waited=%.2f", total_seconds_waited);
    ok(total_seconds_waited <               2.0,                              "Process ended in less than 2 seconds"                            );
    is(WIFEXITED(status),                   1,                                "Process %d is ended!", pid);

    /* TODO - Spawn test: Make sure that we can spawn multiple processes */
    /* TODO - Spawn test: Test child is killed when ctrl-c used on spawn parent */

#endif
    return exit_status();
}
