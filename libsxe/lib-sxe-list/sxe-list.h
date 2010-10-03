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

#ifndef __SXE_LIST_H__
#define __SXE_LIST_H__

#include <stddef.h>
#include <stdlib.h>

#define SXE_LIST_GET_LENGTH(list)       ((list)->length)
#define SXE_LIST_IS_EMPTY(list)         ((list)->length == 0)
#define SXE_LIST_NODE_GET_ID(node)          ((node)->id)

#define SXE_LIST_CONSTRUCT(list, id, type, node) sxe_list_construct_impl((list), (id), offsetof(type, node))

typedef struct SXE_LIST_NODE {
    struct SXE_LIST_NODE * next;
    struct SXE_LIST_NODE * prev;
    unsigned               id;
} SXE_LIST_NODE;

typedef struct SXE_LIST {
    SXE_LIST_NODE sentinel;
    unsigned      length;
    size_t        offset;
} SXE_LIST;

typedef struct SXE_LIST_WALKER {
    SXE_LIST      * list;
    SXE_LIST_NODE * back;
    SXE_LIST_NODE * node;
} SXE_LIST_WALKER;

#include "lib-sxe-list-proto.h"

#endif /* __SXE_LIST_H__ */
