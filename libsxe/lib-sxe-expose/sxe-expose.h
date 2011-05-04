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

#ifndef __SXE_EXPOSE_H__
#define __SXE_EXPOSE_H__

#include <stdbool.h>
#include "sxe-log.h"
#include "sxe.h"

typedef enum SXE_EXPOSE_DATA_TYPE {
    SXE_EXPOSE_UINT32,
    SXE_EXPOSE_INT32,
    SXE_EXPOSE_CHAR_ARY
} SXE_EXPOSE_DATA_TYPE;

typedef struct SXE_EXPOSE_DATA {
    char *               name;
    SXE_EXPOSE_DATA_TYPE type;
    void *               data_ptr;
    unsigned             size;
    const char *         com;
    const char *         perms;
} SXE_EXPOSE_DATA;

extern SXE_EXPOSE_DATA * sxe_expose_gdata;
extern SXE_EXPOSE_DATA * sxe_expose_gdata_tmp;
extern unsigned          sxe_expose_gdata_cnt;

#define SXE_EXPOSE_REG(mname, mtype, msize, mperm, mcom) do {                                                    \
    sxe_expose_gdata_cnt++;                                                                                      \
    sxe_expose_gdata_tmp = malloc(sxe_expose_gdata_cnt * sizeof(SXE_EXPOSE_DATA));                               \
    memcpy(sxe_expose_gdata_tmp, sxe_expose_gdata, (sxe_expose_gdata_cnt - 1) * sizeof(SXE_EXPOSE_DATA));        \
    free(sxe_expose_gdata);                                                                                      \
    sxe_expose_gdata = sxe_expose_gdata_tmp;                                                                     \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].type           = SXE_EXPOSE_##mtype;                              \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].data_ptr       = &mname;                                          \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].size           = msize;                                           \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].name           = strdup(#mname);                                  \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].com            = strdup(mcom);                                    \
    sxe_expose_gdata[sxe_expose_gdata_cnt - 1].perms          = strdup(mperm);                                   \
    } while(0)

#include "lib-sxe-expose-proto.h"

#endif

