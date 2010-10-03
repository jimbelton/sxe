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
    SXEE82("sxe_list_walker_construct(walker=%p,list=%p)", walker, list);
    walker->list = list;
    walker->back = &list->sentinel;
    walker->node = &list->sentinel;
    SXER80("return");
}

/**
 * Go back to the previous node that the walker was pointing to
 *
 * @param walker Pointer to the list walker
 *
 * @return Pointer to the current object or NULL if the walker was previously at the list head or has already been moved back.
 */
void *
sxe_list_walker_back(SXE_LIST_WALKER * walker)
{
    void * result = NULL;

    SXEE81("sxe_list_walker_back(walker=%p)", walker);

    if (walker->back != &walker->list->sentinel) {
        walker->node = walker->back;
        walker->back = &walker->list->sentinel;
        result = (char *)walker->node - walker->list->offset;
    }

    SXER81("return %p", result);
    return result;
}
/**
 * Find the current node that the walker is pointing to
 *
 * @param walker Pointer to the list walker
 *
 * @return Pointer to the current object or NULL if the walker is at the list head.
 */
void *
sxe_list_walker_find(SXE_LIST_WALKER * walker)
{
    void * result = NULL;

    SXEE81("sxe_list_walker_find(walker=%p)", walker);

    if (walker->node != &walker->list->sentinel) {
        result = (char *)walker->node - walker->list->offset;
    }

    SXER81("return %p", result);
    return result;
}

/**
 * Step to the next object in the list
 *
 * @param walker Pointer to the list walker
 *
 * @return Pointer to the next object or NULL if the end of the list has been reached.
 */
void *
sxe_list_walker_step(SXE_LIST_WALKER * walker)
{
    void * result = NULL;

    SXEE81("sxe_list_walker_step(walker=%p)", walker);
    walker->back = walker->node;
    walker->node = SXE_PTR_FIX(walker->list, SXE_LIST_NODE *, walker->node->next);

    if (walker->node != &walker->list->sentinel) {
        result = (char *)walker->node - walker->list->offset;
    }

    SXER81("return %p", result);
    return result;
}
