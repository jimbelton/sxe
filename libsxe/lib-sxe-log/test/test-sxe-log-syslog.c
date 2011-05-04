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
#include <stdarg.h>
#include <string.h>

#ifndef _WIN32
#include <syslog.h>
#endif

#if defined(__APPLE__)
#include <sys/syscall.h>
#elif defined(__FreeBSD__)
#include <sys/thr.h>
#endif

#undef   SXE_DEBUG      /* Since we are testing diagnostic functions, the test program forces debug mode */
#define  SXE_DEBUG 1
#include "sxe-log.h"

#include "mock.h"
#include "tap.h"

#ifdef _WIN32
static unsigned
test_sxe_log_buffer_prefix(char * log_buffer, unsigned id, SXE_LOG_LEVEL level)
{
    unsigned length = 0;
    SXE_UNUSED_ARGUMENT(log_buffer);
    SXE_UNUSED_ARGUMENT(id);
    SXE_UNUSED_ARGUMENT(level);
    return length;
}

int main(void)
{
    plan_skip_all("No syslog() on windows");

    sxe_log_hook_buffer_prefix(NULL);                       /* For coverage */
    sxe_log_hook_buffer_prefix(test_sxe_log_buffer_prefix); /* For coverage */
    return exit_status();
}
#else

static tap_ev_queue q_syslog;
struct object
{
    unsigned id;
};

static void
t_openlog(const char * ident, int option, int facility)
{
    tap_ev_queue_push(q_syslog, "openlog", 3,
                      "ident", tap_dup(ident, strlen(ident)),
                      "option", option,
                      "facility", facility);
}

static void
t_syslog(int priority, const char * format, ...)
{
    va_list ap;
    const char *logline;

    assert(strcmp(format, "%s") == 0);

    va_start(ap, format);
    logline = va_arg(ap, const char *);
    tap_ev_queue_push(q_syslog, "syslog", 2,
                      "priority", priority,
                      "logline", tap_dup(logline, strlen(logline)));
    va_end(ap);
}

int
main(int argc, char *argv[]) {
    tap_ev           ev;
    struct object   self;
    struct object * this = &self;
    char            expected[1024];
#if defined(__FreeBSD__)
    long            tid;
    thr_self(&tid);
#elif defined(__APPLE__)
    pid_t           tid = syscall(SYS_thread_selfid);
#else
    pid_t           tid = getpid();
#endif

    plan_tests(28);

    MOCK_SET_HOOK(openlog, t_openlog);
    MOCK_SET_HOOK(syslog,  t_syslog);
    q_syslog = tap_ev_queue_new();

    sxe_log_use_syslog("my-program", LOG_NDELAY|LOG_PID, LOG_USER);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "openlog",             "sxe_log_use_syslog() calls openlog()");
    is_eq(tap_ev_arg(ev, "ident"), "my-program",        "sxe_log_use_syslog() calls openlog() with correct 'ident' parameter");
    is((int)(uintptr_t)tap_ev_arg(ev, "option"), LOG_NDELAY|LOG_PID,    "sxe_log_use_syslog() calls openlog() with correct 'option' parameter");
    is((int)(uintptr_t)tap_ev_arg(ev, "facility"), LOG_USER,            "sxe_log_use_syslog() calls openlog() with correct 'facility' parameter");

    SXEL10("SXEL10");
    snprintf(expected, sizeof expected, "T=%ld ------ 1 - SXEL10\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL10() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_ERR,             "SXEL10() maps to LOG_ERR syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL10() is logged correctly");

    SXEL21("SXEL21(%s)", "arg1");
    snprintf(expected, sizeof expected, "T=%ld ------ 2 - SXEL21(arg1)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL21() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_WARNING,         "SXEL21() maps to LOG_WARNING syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL21() is logged correctly");

    SXEL32("SXEL32(%s,%d)", "arg1", 22);
    snprintf(expected, sizeof expected, "T=%ld ------ 3 - SXEL32(arg1,22)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL32() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_NOTICE,          "SXEL32() maps to LOG_NOTICE syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL32() is logged correctly");

    SXEL43("SXEL43(%s,%d,%u)", "arg1", 22, 44);
    snprintf(expected, sizeof expected, "T=%ld ------ 4 - SXEL43(arg1,22,44)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL43() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_INFO,            "SXEL43() maps to LOG_INFO syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL43() is logged correctly");

    SXEL54("SXEL54(%s,%d,%u,%x)", "arg1", 22, 44, 64);
    snprintf(expected, sizeof expected, "T=%ld ------ 5 - SXEL54(arg1,22,44,40)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL54() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_DEBUG,           "SXEL54() maps to LOG_DEBUG syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL54() is logged correctly");

    SXEL65("SXEL65(%s,%d,%u,%x,%.2f)", "arg1", 22, 44, 64, 3.1415926);
    snprintf(expected, sizeof expected, "T=%ld ------ 6 - SXEL65(arg1,22,44,40,3.14)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL65() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_DEBUG,           "SXEL65() maps to LOG_DEBUG syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL65() is logged correctly");

    this->id = 99;
    SXEL10I("SXEL10I");
    snprintf(expected, sizeof expected, "T=%ld     99 1 - SXEL10I\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL10I() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_ERR,             "SXEL10I() maps to LOG_ERR syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL10I() is logged correctly");

    this->id = 98;
    SXEL65I("SXEL65I(%s,%d,%u,%x,%.2f)", "arg1", 22, 44, 64, 3.1415926);
    snprintf(expected, sizeof expected, "T=%ld     98 6 - SXEL65I(arg1,22,44,40,3.14)\n", (long)tid);
    ev = tap_ev_queue_shift(q_syslog);
    is_eq(tap_ev_identifier(ev), "syslog",              "SXEL65I() calls syslog()");
    is(tap_ev_arg(ev, "priority"), LOG_DEBUG,           "SXEL65I() maps to LOG_DEBUG syslog level");
    is_eq(tap_ev_arg(ev, "logline"), expected,          "SXEL65I() is logged correctly");

    sxe_log_hook_buffer_prefix(NULL); /* For coverage */
    (void)argc;
    (void)argv;

    return exit_status();
}

#endif /* _WIN32 */
