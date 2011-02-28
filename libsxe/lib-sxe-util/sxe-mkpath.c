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

/* TODO: Rewrite to avoid using "system()" */

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "sxe-log.h"
#include "sxe-util.h"

#ifdef _WIN32
#   define MKDIR_COMMAND "mkdir"
#else
#   define MKDIR_COMMAND "mkdir -p"
#endif

SXE_RETURN
sxe_mkpath(const char * path)
{
    SXE_RETURN  result = SXE_RETURN_ERROR_INTERNAL;
    char        command[PATH_MAX + sizeof(MKDIR_COMMAND) + 1];
    struct stat filestat;
#ifdef _WIN32
    unsigned    command_length;
    unsigned    i;
#endif

    SXEA10(snprintf(command, sizeof(command), MKDIR_COMMAND " %s", path) >= 0, "Failed to format 'mkdir' command");

#ifdef _WIN32
    /* Remove slash termination (tossers at MS don't implement stat correctly)
     */
    command_length = strlen(command) - 1;

    if (command[command_length] == '/') {
        command[command_length] = '\0'; /* Coverage Exclusion - todo: win32 coverage */
    }

    for (i = command_length - 1; i >= sizeof(MKDIR_COMMAND); i--) {
        if (command[i] == '/') {
            command[i] = '\\';
        }
    }
#endif

    SXEL82("sxe_mkpath(): command '%s'", command, &command[sizeof(MKDIR_COMMAND)]);

    if (stat(&command[sizeof(MKDIR_COMMAND)], &filestat) >= 0) {
        SXEL81("sxe_mkpath(): '%s' already exists", path);
        result = SXE_RETURN_OK; /* COVERAGE EXCLUSION - harmless, and only happens on windows */
        goto SXE_EARLY_OUT;     /* COVERAGE EXCLUSION - harmless, and only happens on windows */
    }

    if (system(command) == 0) {
        result = SXE_RETURN_OK;
    }
SXE_EARLY_OUT:
    return result;
}
