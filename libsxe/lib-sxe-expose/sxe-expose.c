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
#include <ctype.h>

#include "sxe-log.h"
#include "sxe.h"
#include "sxe-expose.h"

#define COMMAND_DELIM '='

SXE_EXPOSE_DATA * sxe_expose_gdata     = NULL;
SXE_EXPOSE_DATA * sxe_expose_gdata_tmp = NULL;
unsigned          sxe_expose_gdata_cnt = 0;

typedef enum ACTION {
    GET,
    SET
} ACTION;


static int
isCvarChar(char c)
{
    return isalnum(c) || c == '_';
}


static void
sxe_expose_peform_set(int i, const char * val, unsigned val_len, char * buf, unsigned buf_len)
{
    char scratch[1024];

    SXEE80("sxe_expose_perform_set()");

    if (strchr(sxe_expose_gdata[i].perms, 'W') == NULL ) {
        snprintf(buf, buf_len, "Variable '%s' is not writable\n", sxe_expose_gdata[i].name);
        goto SXE_EARLY_OUT;
    }

    switch (sxe_expose_gdata[i].type) {

    case SXE_EXPOSE_UINT32:
        snprintf(scratch, sizeof(scratch), "%.*s", val_len, val);
        if (sscanf(scratch, "%u", (uint32_t *)sxe_expose_gdata[i].data_ptr) == 1) {
            snprintf(buf, buf_len, "%s=%u\n", sxe_expose_gdata[i].name, *(uint32_t *)sxe_expose_gdata[i].data_ptr);
        } else {
            snprintf(buf, buf_len, "Failed to set '%s' to '%.*s'\n", sxe_expose_gdata[i].name, val_len, val); /* Coverage Exclusion */
        }
        break;

    case SXE_EXPOSE_INT32:
        snprintf(scratch, sizeof(scratch), "%.*s", val_len, val);
        if (sscanf(scratch, "%d", (int32_t *)sxe_expose_gdata[i].data_ptr) == 1) {
            snprintf(buf, buf_len, "%s=%d\n", sxe_expose_gdata[i].name, *(int32_t *)sxe_expose_gdata[i].data_ptr);
        } else {
            snprintf(buf, buf_len, "Failed to set '%s' to '%.*s'\n", sxe_expose_gdata[i].name, val_len, val); /* Coverage Exclusion */
        }
        break;

    case SXE_EXPOSE_CHAR_ARY:
        if (val_len >= sxe_expose_gdata[i].size) {
            snprintf(buf, buf_len, "Failed %s=\"%.*s\", array too short\n", sxe_expose_gdata[i].name, val_len, val);
        } else {
            memcpy((char *)sxe_expose_gdata[i].data_ptr, val, val_len);
            ((char *)sxe_expose_gdata[i].data_ptr)[val_len] = '\0';
            snprintf(buf, buf_len, "%s=\"%s\"\n", sxe_expose_gdata[i].name, (char *)sxe_expose_gdata[i].data_ptr);
        }
        break;

    default:
        SXEA11(0, "Unknown exposed type: '%d'", sxe_expose_gdata[i].type); /* Coverage Exclusion - case default assert */
    }

SXE_EARLY_OUT:
    SXER80("return");
}


static void
sxe_expose_peform_get(int i, char * buf, unsigned buf_len)
{
    SXEE80("sxe_expose_perform_get()");

    if (strchr(sxe_expose_gdata[i].perms, 'R') == NULL ) {
        snprintf(buf, buf_len, "Variable '%s' is not readable\n", sxe_expose_gdata[i].name);
        goto SXE_EARLY_OUT;
    }

    switch (sxe_expose_gdata[i].type) {
    case SXE_EXPOSE_UINT32:
        snprintf(buf, buf_len, "%s=%u\n", sxe_expose_gdata[i].name, *(uint32_t *)sxe_expose_gdata[i].data_ptr);
        break;

    case SXE_EXPOSE_INT32:
        snprintf(buf, buf_len, "%s=%d\n", sxe_expose_gdata[i].name, *(int32_t *)sxe_expose_gdata[i].data_ptr);
        break;

    case SXE_EXPOSE_CHAR_ARY:
        snprintf(buf, buf_len, "%s=\"%.*s\"\n", sxe_expose_gdata[i].name, sxe_expose_gdata[i].size, (char *)sxe_expose_gdata[i].data_ptr);
        break;

    default:
        SXEA11(0, "Unknown exposed type: '%d'", sxe_expose_gdata[i].type); /* Coverage Exclusion - Default assert */
    }

SXE_EARLY_OUT:
    SXER80("return");
}

static const char *
typTOstr(SXE_EXPOSE_DATA_TYPE t)
{
    const char * result = "Unknown type";

    if (t == SXE_EXPOSE_UINT32) {
        result = "SXE_EXPOSE_UINT32";
    } else if (t == SXE_EXPOSE_INT32) {
        result = "SXE_EXPOSE_INT32";
    } else if (t == SXE_EXPOSE_CHAR_ARY) {
        result = "SXE_EXPOSE_CHAR_ARY";
    } else {
        SXEA11(0, "Unknown type: %d", t); /* Coverage Exclusion */
    }
    return result;
}

static void
sxe_expose_dump(char * buf, unsigned buf_len)
{
    unsigned i;
    unsigned buf_used = 0;

    SXEE80("sxe_expose_dump()");

    for(i = 0; i < sxe_expose_gdata_cnt; i++) {
        buf_used += snprintf(buf + buf_used, buf_len - buf_used, "%s, %s, %u, %s, \"%s\"\n", sxe_expose_gdata[i].name,
                             typTOstr(sxe_expose_gdata[i].type), sxe_expose_gdata[i].size, sxe_expose_gdata[i].perms, sxe_expose_gdata[i].com);
    }

    SXER80("return");
}


void
sxe_expose_parse(const char * cmd, char * buf, unsigned buf_len)
{
    ACTION       action;
    unsigned     i = 0;
    const char * key     = NULL;
    unsigned     key_len = 0;
    const char * val     = NULL;
    unsigned     val_len = 0;
    char         val_delim;

    SXEE81("sxe_expose_parse(%s)", cmd);

    // trim leading white space
    while (isspace(cmd[i]) && cmd[i] != '\0') { i++; }
    if (cmd[i] == '\0') { goto SXE_ERROR_OUT; }
    key = cmd + i;
    SXEL90("Finished trimming white space");

    i = 0;
    // commands/keys always start with a letter...
    if (!isalpha(key[i])) { goto SXE_ERROR_OUT; }
    SXEL90("Found the start of the key");

    i = 0;
    while (key[i] != COMMAND_DELIM && key[i] != '\0' && isCvarChar(key[i])) { i++; }
    key_len = i;

    if (key[i] != COMMAND_DELIM && (isspace(key[i]) || key[i] == '\0')) {
        if (strncmp(key, "DUMP", key_len) == 0) {
            sxe_expose_dump(buf, buf_len);
            goto SXE_EARLY_OUT;
        }
        action = GET;
        SXEL92("Performing a 'GET' on '%.*s'", key_len, key);
    }
    else if (key[i] != COMMAND_DELIM) {
        SXEL91("Did not find the deliminator or space or null, but found none alphanumeric char?: '%c'", key[i]);
        goto SXE_ERROR_OUT;
    }
    else {
        action = SET;
        val = key + i + 1;
        i = 0;
        if (!isdigit(*val) && *val != '-') { /* val is a string */
            val_delim = *val;
            if (isspace(val_delim) || val_delim == '\0') { goto SXE_ERROR_OUT; }
            val++;
            while (val[i] != val_delim && val[i] != '\0') { i++; }
            if (val[i] != val_delim) { goto SXE_ERROR_OUT; }
            val_len = i;
        } else { /* val is numeric */
            i++; /* first digit already validated, and could have been a '-' sign */
            while (isdigit(val[i])) { i++; }
            val_len = i;
        }
        if (val_len == 0) { goto SXE_ERROR_OUT; }
        SXEL94("Performing a 'SET' on '%.*s' with '%.*s'", key_len, key, val_len, val);
    }

    for(i = 0; i < sxe_expose_gdata_cnt; i++) {
        if (strncmp(key, sxe_expose_gdata[i].name, key_len) == 0 && strlen(sxe_expose_gdata[i].name) == key_len) { break; }
        if (i == sxe_expose_gdata_cnt - 1) {
            snprintf(buf, buf_len, "Variable not exposed: %.*s\n", key_len, key);
            goto SXE_EARLY_OUT;
        }
    }

    if (action == GET) {
        sxe_expose_peform_get(i, buf, buf_len);
    } else { /* SET */
        sxe_expose_peform_set(i, val, val_len, buf, buf_len);
    }

    goto SXE_EARLY_OUT;

SXE_ERROR_OUT:
    if (key == NULL) {
        strncpy(buf, "No command\n", buf_len);
    } else {
        snprintf(buf, buf_len, "Bad command: %s\n", key);
    }

SXE_EARLY_OUT:
    SXER81("return: '%s'", buf);
    return;
}

