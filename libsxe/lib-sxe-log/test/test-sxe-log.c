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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if (SXE_DEBUG == 1)
#define LOCAL_SXE_DEBUG 1
#endif

#undef   SXE_DEBUG      /* Since we are testing diagnostic functions, the test program forces debug mode */
#define  SXE_DEBUG 1
#include "sxe-log.h"
#include "tap.h"

/* Make putenv STFU about const strings
 */
#define PUTENV(string) putenv((char *)(long)(string))

struct object
{
    unsigned id;
};

static unsigned     test_state = 0;
static const char   entering[] = "Entering the log test";
static const char   escape[]   = "Line 1: high bit character \x80\r\nLine 2: This is a hex character (BS): \b\r\nLine 3: This is a backslash: \\\r\n";
static const char   escaped[]  = "Line 1: high bit character \\x80\\r\\nLine 2: This is a hex character (BS): \\x08\\r\\nLine 3: This is a backslash: \\\\\\r\\n\n";
static const char   logging[]  = "Logging a normal line";
static const char   truncate[] = "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789"
                                 "0123456789112345678921234567893123456789412345678951234567896123456789712345678981234567899123456789";
static const char   hextrunc[] = "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                                 "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
static const char   idlevin[]  = "    99 6   - ";
static const char   dumpdata[] = "test";
static const char   exiting[]  = "Exiting the log test";
static const char   dumphex[]  = "74 65 73 74";

static void
log_line(SXE_LOG_LEVEL level, char * line)
{
    char buf[256];

    switch (test_state) {
    case 0:
        is(level, SXE_LOG_LEVEL_TRACE, "Line was logged at TRACE level");
        is_strncmp(&line[strlen(line) - strlen(entering) - 1], entering, strlen(entering), "Test line 0 ends with '%s'", entering);
        break;

    case 1:
        ok(strstr(line, __FILE__) != NULL, "Test line 1 includes file name '%s'", __FILE__);
        break;

    case 2:
        is_strncmp(&line[strlen(line) - strlen(logging) - 1], logging, strlen(logging), "Test line 2 ends with '%s'", logging);
        is_strncmp(&line[strlen(line) - strlen(logging) - strlen(idlevin) - 1], idlevin, strlen(idlevin), "Test line 2 has id 99, level 6 and indent 2");
        break;

    case 3:
        ok(strstr(line, dumpdata) != NULL,
           "Test line 3 contains '%s'", dumpdata);
        ok(strstr(line, dumphex) != NULL,
           "Test line 3 contains hex '%s'", dumphex);
        break;

    case 4:
        is_strncmp(&line[strlen(line) - strlen(exiting) - 1], exiting, strlen(exiting), "Test line 4 ends with '%s'", exiting);
        break;

    case 5:
        strcpy(buf, "} // ");
        strcat(buf, __FILE__);
        ok(strstr(line, buf) != NULL, "Test line 5 includes end of block with file name '%s'", buf);
        break;

    case 6:
        is(strlen(line), 1023, "Very long log line truncated");
        is_eq(&line[1020], "..\n", "Truncation indicator found");
        break;

    case 7:
        is(strlen(line), 1023, "Very long entry line truncated");
        break;

    case 8:
        ok(strstr(line, __FILE__) != NULL, "Test line 8 includes file name '%s'", __FILE__);
        break;

    case 9:
        is_eq(&line[strlen(line) - strlen(escaped)], escaped, "Test line 4 ends with '%.*s'", (int)strlen(escaped) - 1, escaped);
        break;

    case 10:
        is(strlen(line), 1023, "Line of 300 backspaces escaped and truncated");
        break;

    case 11:
        /* Doesn't crash */
        break;

    case 12:
        is(level, SXE_LOG_LEVEL_FATAL, "Assertions are logged at level FATAL");
        ok((strstr(line, "ERROR: debug assertion 'this != &self' failed at test/test-sxe-log.c") != NULL) ||
           (strstr(line, "ERROR: debug assertion 'this != &self' failed at sxe/lib-sxe-log/test/test-sxe-log.c") != NULL),
           "Assertion includes expected stringized test");
#ifdef WINDOWS_NT
        diag("info: You can expect and ignore a message saying:");
        diag("    > This application has requested the Runtime to terminate it in an unusual way.");
        diag("    > Please contact the application's support team for more information.");
#endif
        break;

    default:
        diag("Unexpected test sequence number %u.\n", test_state);
        exit(1);
    }

    test_state++;
}

static void
test_abort_handler(int sig)
{
    (void)sig;
    is(test_state, 13, "All log lines/signals received");
    exit(exit_status());
}

#define TEST_CASE_RETURN_TO_STRING(id) is_eq(sxe_return_to_string(SXE_RETURN_##id), #id, "test return string " #id)

int
main(int argc, char *argv[]) {
    struct object   self;
    struct object * this = &self;

    (void)argc;
    (void)argv;

    plan_tests(35);

    /* sxe_return_to_string() */
    is_eq(sxe_return_to_string(SXE_RETURN_OK),             "OK"            , "sxe_return_to_string(SXE_RETURN_OK) eq \"OK\"");
    is_eq(sxe_return_to_string(SXE_RETURN_ERROR_INTERNAL), "ERROR_INTERNAL", "sxe_return_to_string(SXE_RETURN_ERROR_INTERNAL) eq \"ERROR_INTERNAL\"");
    is_eq(sxe_return_to_string(~0U),                       "INVALID_VALUE",  "sxe_return_to_string(~0U) eq \"INVALID_VALUE\"");

    TEST_CASE_RETURN_TO_STRING(WARN_CACHE_DOUBLE_INITIALIZED);
    TEST_CASE_RETURN_TO_STRING(WARN_WOULD_BLOCK);
    TEST_CASE_RETURN_TO_STRING(WARN_ALREADY_CLOSED);
    TEST_CASE_RETURN_TO_STRING(ERROR_CACHE_UNINITIALIZED);
    TEST_CASE_RETURN_TO_STRING(ERROR_ALLOC);
    TEST_CASE_RETURN_TO_STRING(ERROR_NO_CONNECTION);
    TEST_CASE_RETURN_TO_STRING(ERROR_ALREADY_CONNECTED);
    TEST_CASE_RETURN_TO_STRING(ERROR_INVALID_URI);
    TEST_CASE_RETURN_TO_STRING(ERROR_ADDRESS_IN_USE);
    TEST_CASE_RETURN_TO_STRING(ERROR_INTERRUPTED);
    TEST_CASE_RETURN_TO_STRING(ERROR_COMMAND_NOT_RUN);
    TEST_CASE_RETURN_TO_STRING(UNCATEGORIZED);
    TEST_CASE_RETURN_TO_STRING(INVALID_VALUE);    /* Just for coverage */

    ok(signal(SIGABRT, test_abort_handler) != SIG_ERR, "Caught abort signal");
    sxe_log_hook_line_out(NULL); /* for coverage */
    sxe_log_hook_line_out(log_line);
    PUTENV("SXE_LOG_LEVEL=6");   /* Trigger processing of the level in the first call to the log */
    SXEE60(entering);
    PUTENV("SXE_LOG_LEVEL=1");   /* This should be ignored. If it is not, the tests will fail    */
    this->id = 99;
    SXEL60I(logging);
    SXEA60(1, "Asserting true");
    SXED60(dumpdata, 4);
    SXER60(exiting);
    SXEL60(truncate);
    SXEE61("really long entry message: %s", truncate);
    SXEL60(escape);
    SXEL60(hextrunc);
    SXED60(dumpdata, 0);    /* Edge case */
    SXEA80(1, "We should not get this, because level 8 is too low!");
    sxe_log_init();         /* For Coverage */

#if defined(_WIN32) && defined(LOCAL_SXE_DEBUG)
    skip(3, "Can't test aborts in a Windows debug build, due to pop up Window stopping the build");
#else
    SXEA60(this != &self, "This is not self");  /* Abort - must be the last thing we do*/
    fail("Did not catch an abort signal");
#endif
    return exit_status();
}
