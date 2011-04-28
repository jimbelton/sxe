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
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if SXE_DEBUG != 0
#define LOCAL_SXE_DEBUG 1
#endif

#undef   SXE_DEBUG      /* Since we are testing diagnostic functions, this test program forces debug mode */
#define  SXE_DEBUG 1

#include "sxe-log.h"
#include "tap.h"

static const char * test_program_name;
static const char * test_program_arg;
static FILE       * test_output_pipe = NULL;

static void
test_log_line_out_to_stdout(SXE_LOG_LEVEL level, const char * line)
{
    SXE_UNUSED_ARGUMENT(level);
    fputs(line, stdout);
    fflush(stdout);
}

static char *
test_log_next(void)
{
    static char line[512];
    char *      result;

    SXEA12(test_output_pipe != NULL, "test_log_next: No test program is running: trying running '%s %s' manually",
           test_program_name, test_program_arg);

    if ((result = fgets(line, sizeof(line), test_output_pipe)) == NULL) {
        SXEA12(pclose(test_output_pipe) >= 0, "%s: Failed to pclose: %s", test_program_name, strerror(errno));
        test_output_pipe = NULL;
    }

    return result;
}

static char *
test_log_first(const char * arg)
{
    char buffer[512];

    if (test_output_pipe != NULL) {
        SXEA12(pclose(test_output_pipe) >= 0, "%s: Failed to pclose: %s", test_program_name, strerror(errno));
    }

    test_program_arg           = arg;
    buffer[sizeof(buffer) - 1] = '\0';
    strncpy(buffer, test_program_name, sizeof(buffer) - 1);
    strncat(buffer, " ",               sizeof(buffer) - 1);
    strncat(buffer, arg,               sizeof(buffer) - 1);
    SXEA12((test_output_pipe = popen(buffer, "r")) != NULL, "Failed to popen '%s': %s", buffer, strerror(errno));
    return test_log_next();
}

#define TEST_LINES_EXPECTED (sizeof(test_expected) / sizeof(test_expected[0]))

static const char * test_expected[] = {
    "2 - No indent",
    "2   - Indented by 2",
    "2       - Set log level to 9",
    "8       - return // eight",
    "6     - return // six",
    "5   - return // five",
    "5 + level_five()",
    "6   + test_level_six(",
    "8     + test_level_eight(",
    "2 - Set log level to 3"
};

void test_level_six(SXE_LOG_LEVEL level);    /* Function defined in the test/sxe-log-levels.c helper module */

int
main(int argc, char ** argv)
{
    const char * line;
    unsigned     i;

    if (argc > 1) {
        test_program_arg = argv[1];
        sxe_log_hook_line_out(test_log_line_out_to_stdout);
        SXEA10(sxe_log_hook_line_out(test_log_line_out_to_stdout) == test_log_line_out_to_stdout,
               "sxe_log_hook_line_out failed to hook test_log_line_out_to_stdout");

        if (strcmp(argv[1], "1") == 0) {
            if (getenv("SXE_LOG_LEVEL") != NULL) {
                SXED80("should not see this (level too high)", strlen("should not see this (level too high)"));    /* Cover early out in dump */
            }

            SXEL40("BOO");
        }
        else if (strcmp(argv[1], "2") == 0) {
#ifndef LOCAL_SXE_DEBUG
            sxe_log_set_indent_maximum(~0U);
#endif
            sxe_log_set_level(SXE_LOG_LEVEL_WARNING);
            SXEE50("level_five()");
            test_level_six(SXE_LOG_LEVEL_LIBRARY_DUMP);
            SXER50("return // five");
            SXEE50("level_five()");
            test_level_six(SXE_LOG_LEVEL_WARNING);
            SXER50("return // five");
            SXEL20("that's all, folks");
        }

        exit(0);
    }

    test_program_name = argv[0];
    plan_tests(3 * TEST_LINES_EXPECTED + 3);

    /* Tests for different log level settings */

    tap_test_case_name("Level settings");
    ok((line = test_log_first("1")) != NULL,                        "Test log at default level wrote a line");
    diag("line = %s", line);

    SXEA12(putenv((char *)(intptr_t)"SXE_LOG_LEVEL=2") >= 0,        "%s: Failed to putenv: %s", test_program_name, strerror(errno));
    ok(test_log_first("1") == NULL,                                 "Test log with SXE_LOG_LEVEL=2 failed to write a line");

    /* TODO: Replace putenvs with calls to the TBD function that allows setting fine grained levels programmatically. */

    SXEA12(putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE=5") >= 0, "%s: Failed to putenv: %s", test_program_name, strerror(errno));
    ok((line = test_log_first("1")) != NULL,                        "Test log with SXE_LOG_LEVEL_LIBSXE=5 wrote a line");
    diag("line = %s", line);
    SXEA12(putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG=2") >= 0, "%s: Failed to setenv: %s", test_program_name, strerror(errno));
    ok(test_log_first("1") == NULL,                                 "Test log with SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG=2 failed to write a line");
    SXEA12(putenv((char *)(intptr_t)"SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG_TEST_TEST_SXE_LOG_LEVELS=7") >= 0, "%s: Failed to putenv: %s", test_program_name, strerror(errno));
    ok((line = test_log_first("1")) != NULL,                        "Test log with SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG_TEST_TEST_SXE_LOG_LEVELS=7 wrote a line");
    diag("line = %s", line);

    /* Remove the more specific environment variables
     */
    SXEA12(unsetenv("SXE_LOG_LEVEL_LIBSXE") == 0,                                      "%s: unsetenv failed: %s", test_program_name, strerror(errno));
    SXEA12(unsetenv("SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG") == 0,                          "%s: unsetenv failed: %s", test_program_name, strerror(errno));
    SXEA12(unsetenv("SXE_LOG_LEVEL_LIBSXE_LIB_SXE_LOG_TEST_TEST_SXE_LOG_LEVELS") == 0, "%s: unsetenv failed: %s", test_program_name, strerror(errno));

    /* Tests for indentation interacting with log level */

    tap_test_case_name("Indentation");
    line = test_log_first("2");

    for (i = 0; i < TEST_LINES_EXPECTED; i++) {
        ok(line != NULL,                                            "Got line %u", 2 * i + 1);
        ok(strstr(line, test_expected[i]) != NULL,                  "Found '%s' in '%.*s'", test_expected[i], (int)strlen(line) - 1, line);

        if (i > 2) {
            ok(test_log_next() != NULL,                             "Got line %u", 2 * i + 2);
        }

        line = test_log_next();
    }

    ok(line == NULL,                                                "Got EOF");
    return exit_status();
}
