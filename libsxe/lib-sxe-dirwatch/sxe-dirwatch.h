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

#ifndef __SXE_DIRWATCH_H__
#define __SXE_DIRWATCH_H__

/* TODO: implement sxe_dirwatch on windows */

#define SXE_DIRWATCH_CREATED      1        /* file was created (or moved into directory via rename) */
#define SXE_DIRWATCH_MODIFIED     2        /* file was modified */
#define SXE_DIRWATCH_DELETED      4        /* file was deleted (or moved out of diretory via rename) */

/* A watched directory object
 */
typedef struct SXE_DIRWATCH {
    int           fd;
    void       (* notify)(EV_P_ const char * file, int revents, void * user_data);
    void        * user_data;
    SXE_LIST_NODE node;
} SXE_DIRWATCH;

#include "sxe-dirwatch-proto.h"

#endif  /* __SXE_DIRWATCH_H__ */
