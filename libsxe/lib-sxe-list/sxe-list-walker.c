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

#include "sxe-list.h"
#include "sxe-log.h"
#include "sxe-util.h"

/**
 * Construct a list walker
 *
 * @param walker Pointer to the list walker
 * @param list   Pointer to the list to walk
 */
void
sxe_list_walker_construct(SXE_LIST_WALKER * walker, SXE_LIST * list)
{
    walker->list = list;
    walker->node = &list->sentinel;
}

/**
 * Step to the next node in the list
 *
 * @param walker Pointer to the list walker
 *
 * @return Pointer to the next node or NULL if the end of the list has been reached.
 */
SXE_LIST_NODE *
sxe_list_walker_step(SXE_LIST_WALKER * walker)
{
    walker->node = SXE_PTR_FIX(walker->list, SXE_LIST_NODE *, walker->node->next);

    if (walker->node == &walker->list->sentinel) {
        return NULL;
    }

    return walker->node;
}

/**
 * Visit the objects in a list until a visit returns a non-zero value or every object has been visited
 *
 * @param list         The list to walk
 * @param visit        Function to call on each object
 * @param user_data    Arbitrary value to pass to visit function (e.g value to search for)
 *
 * @return NULL or non-NULL value returned from visit (indicates that the walk was stopped early - e.g. pointer to object found)
 */
void *
sxe_list_walk(SXE_LIST * list, void * (*visit)(void * object, void * user_data), void * user_data)
{
    SXE_LIST_WALKER walker;
    SXE_LIST_NODE * node;
    void          * result = NULL;

    SXEE83("sxe_list_walk(list=%p,visit=%p,user_data=%p)", list, visit, user_data);

    sxe_list_walker_construct(&walker, list);

    for (node = sxe_list_walker_step(&walker); node != NULL; node = sxe_list_walker_step(&walker)) {
        if ((result = (*visit)((char *)node - list->offset, user_data)) != 0) {
            break;
        }
    }

    SXER81("return result=%p", result);
    return result;
}
