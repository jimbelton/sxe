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

#include "sxe-log.h"
#include "tap.h"

static char * test_program_name;

static bool
test_log(void)
{
    char   buffer[512];
    FILE * output;
    bool   result;

    buffer[sizeof(buffer) - 1] = '\0';
    strncpy(buffer, test_program_name, sizeof(buffer) - 1);
    strncat(buffer, " -", sizeof(buffer) - 1);
    SXEA12((output = popen(buffer, "r")) != NULL, "Failed to popen '%s': %s", buffer, strerror(errno));
    result = fgets(buffer, sizeof(buffer), output) != NULL;
    SXEA12(pclose(output) >= 0,                   "%s: Failed to pclose: %s", test_program_name, strerror(errno));
    return result;
}

int
main(int argc, char ** argv)
{
    if (argc > 1) {
        close(fileno(stderr));
        dup2(fileno(stdout), fileno(stderr));
        SXEL40("BOO");
        exit(0);
    }

    test_program_name = argv[0];
    plan_tests(2);
    ok(test_log(),                                              "Test log succeeded");
    SXEA12(setenv("SXE_LOG_LEVEL", "2", 1) >= 0,                "%s: Failed to setenv: %s", test_program_name, strerror(errno));
    ok(!test_log(),                                             "Test log with SXE_LOG_LEVEL=2 failed");
//    SXEA12(setenv("SXE_LIBSXE_LOG_LEVEL", "2", 1) >= 0,         "%s: Failed to setenv: %s", test_program_name, strerror(errno));
//    ok(test_log(),                                              "Test log with SXE_LIBSXE_LOG_LEVEL=7 succeeded");
    return exit_status();
}
