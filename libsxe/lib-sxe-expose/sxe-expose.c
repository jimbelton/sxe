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

#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include "sxe-log.h"
#include "sxe.h"
#include "sxe-expose.h"

#define COMMAND_DELIM " "

SXE_EXPOSE_DATA * sxe_expose_gdata     = NULL;
SXE_EXPOSE_DATA * sxe_expose_gdata_tmp = NULL;
unsigned          sxe_expose_gdata_cnt = 0;

typedef enum ACTION {
    SET,
    GET
} ACTION;


static void
sxe_expose_run_cmd(ACTION action, int i, int data, char * buf, unsigned buf_len)
{
    switch (sxe_expose_gdata[i].type) {
    case SXE_EXPOSE_UINT32:
        if (action == GET) {
            snprintf(buf, buf_len, "%u", *(uint32_t *)sxe_expose_gdata[i].data_ptr);
        } else {
            *(uint32_t *)sxe_expose_gdata[i].data_ptr = (uint32_t)data;
            if (buf_len) { buf[0] = '\0'; }
        }
        break;
    default:
        SXEA11(0, "Unknown exposed type: '%d'", sxe_expose_gdata[i].type); /* Coverage Exclusion - Default assert */
    }
}

void
sxe_expose_parse(const char * command, char * buf, unsigned buf_len)
{
    ACTION       action;
    char       * cmd    = NULL;
    char       * result = NULL;
    unsigned     i;

    SXEE81("sxe_expose_parse(%s)", command);
    cmd = strdup(command);

    result = strtok(cmd, COMMAND_DELIM);
    if (result == NULL) { goto SXE_EARLY_OUT; }
    SXEL91("action='%s'", result);

    if      (strcmp(result, "set") == 0) { action = SET; }
    else if (strcmp(result, "get") == 0) { action = GET; }
    else                                 { strncpy(buf, "Unknown action", buf_len); goto SXE_EARLY_OUT; }

    result = strtok(NULL, COMMAND_DELIM);
    if (result == NULL) { goto SXE_EARLY_OUT; }
    SXEL91("variable='%s'", result);

    for(i = 0; i < sxe_expose_gdata_cnt; i++) {
        if (strcmp(result, sxe_expose_gdata[i].name) == 0) { break; }
        if (i == sxe_expose_gdata_cnt - 1) { strncpy(buf, "Variable not exposed", buf_len); goto SXE_EARLY_OUT; }
    }

    if (action == GET) {
        sxe_expose_run_cmd(action, i, 0, buf, buf_len);
        goto SXE_EARLY_OUT;
    }

    result = strtok(NULL, COMMAND_DELIM);
    if (result == NULL) { strncpy(buf, "Missing parameter for set", buf_len); goto SXE_EARLY_OUT; }
    SXEL91("data='%s'", result);

    sxe_expose_run_cmd(action, i, atoi(result), buf, buf_len);

SXE_EARLY_OUT:
    free(cmd);
    SXER80("return");
    return;
}

