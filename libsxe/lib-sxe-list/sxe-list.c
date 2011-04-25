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

/* Define some macros to hopefully simplify the position indendependence
 */
#define NULL_REL                         SXE_PTR_REL(list, void *, NULL)
#define NODE_PTR_FIX(node)               SXE_PTR_FIX(list, SXE_LIST_NODE *, node)
#define SENTINEL_PTR_REL(list)           SXE_PTR_REL(list, SXE_LIST_NODE *, &list->sentinel)

/* For readability, define these pseudo members for the list structure.
 */
#define HEAD sentinel.next
#define TAIL sentinel.prev

/* List link prev pointers point headwards, and next pointers point tailwards. */

/**
 * Construct a list object.
 *
 * @note Trace logged at dump level to prevent flooding the logs at trace level on startup
 */
void
sxe_list_construct_impl(SXE_LIST * list, unsigned id, size_t offset)
{
    SXEE92("sxe_list_construct_impl(list=%p, id=%u, offset=%u)", list, offset);
    list->HEAD          = SENTINEL_PTR_REL(list);
    list->TAIL          = SENTINEL_PTR_REL(list);
    list->sentinel.id   = id;
    list->length        = 0;
    list->offset        = offset;
    SXER90("return");
}

/**
 * Push an object onto the tail of a list
 */
void
sxe_list_push(SXE_LIST * list, void * object)
{
    SXE_LIST_NODE * node;

    SXEE82("sxe_list_push(list=%p,object=%p)", list, object);
    node             = (SXE_LIST_NODE *)((char *)object + list->offset);
    node->next                     = SENTINEL_PTR_REL(list);
    node->prev       = list->TAIL;
    list->TAIL                     = SXE_PTR_REL(list, SXE_LIST_NODE *, node);
    NODE_PTR_FIX(node->prev)->next = SXE_PTR_REL(list, SXE_LIST_NODE *, node);
    node->id                       = list->sentinel.id;
    list->length++;
    SXER80("return");
}

/**
 * Unshift an object onto the head of a list
 */
void
sxe_list_unshift(SXE_LIST * list, void * object)
{
    SXE_LIST_NODE * node;

    SXEE82("sxe_list_unshift(list=%p,object=%p)", list, object);
    node = (SXE_LIST_NODE *)((char *)object + list->offset);
    node->prev                     = SENTINEL_PTR_REL(list);
    node->next                     = list->HEAD;
    list->HEAD                     = SXE_PTR_REL(list, SXE_LIST_NODE *, node);
    NODE_PTR_FIX(node->next)->prev = SXE_PTR_REL(list, SXE_LIST_NODE *, node);
    node->id                       = list->sentinel.id;
    list->length++;
    SXER80("return");
}

/**
 * Remove an object from a list (returning a pointer to it)
 */
void *
sxe_list_remove(SXE_LIST * list, void * object)
{
    SXE_LIST_NODE * node;

    SXEE82("sxe_list_remove(list=%p,object=%p)", list, object);
    SXEA13((list->HEAD != SENTINEL_PTR_REL(list)) && (list->TAIL != SENTINEL_PTR_REL(list)),
           "Can't remove an object from an empty list; list->HEAD %u, list->TAIL %u, SENTINEL_PTR_REL(list) %u",
           list->HEAD, list->TAIL, SENTINEL_PTR_REL(list));
    SXEA13((list->HEAD != SENTINEL_PTR_REL(list)) || (list->TAIL != SENTINEL_PTR_REL(list)),
           "List is in an inconsistant state; list->HEAD %u, list->TAIL %u, SENTINEL_PTR_REL(list) %u",
           list->HEAD, list->TAIL, SENTINEL_PTR_REL(list));
    node = (SXE_LIST_NODE *)((char *)object + list->offset);
    SXEA84(node->id == list->sentinel.id, "Node %p on list %p has id %u but list has %u", node, list, node->id, list->sentinel.id);
    SXEA10((node->next != NULL_REL) && (node->prev != NULL_REL), "Node is not on a list");

    NODE_PTR_FIX(node->next)->prev = node->prev;
    NODE_PTR_FIX(node->prev)->next = node->next;
    node->prev                     = NULL_REL;
    node->next                     = NULL_REL;
    list->length--;
    SXER81("return object=%x", object);
    return object;
}

/**
 * Pop an object off the tail of a list
 */
void *
sxe_list_pop(SXE_LIST * list)
{
    SXE_LIST_NODE * node = NODE_PTR_FIX(list->TAIL);
    void          * result;

    SXEE81("sxe_list_pop(list=%p)", list);

    if (node == &list->sentinel) {
        SXEA11(list->HEAD == SENTINEL_PTR_REL(list), "List has no element at its tail, but has %p at its head",
               NODE_PTR_FIX(list->HEAD));
        result = NULL;
        goto SXE_ERROR_OUT;
    }

    result = sxe_list_remove(list, (char *)node - list->offset);

SXE_ERROR_OUT:
    SXER81("return %p", result);
    return result;
}

/**
 * Shift an object off the head of a list
 */
void *
sxe_list_shift(SXE_LIST * list)
{
    SXE_LIST_NODE * node = NODE_PTR_FIX(list->HEAD);
    void          * result;

    SXEE81("sxe_list_shift(list=%p)", list);

    if (node == &list->sentinel) {
        SXEA11(list->TAIL == SENTINEL_PTR_REL(list), "List has no element at its head, but has %p at its tail",
               NODE_PTR_FIX(list->TAIL));
        result = NULL;
        goto SXE_ERROR_OUT;
    }

    result = sxe_list_remove(list, (char *)node - list->offset);

SXE_ERROR_OUT:
    SXER81("return %p", result);
    return result;
}

/**
 * Peek at the object at the head of a list
 */
void *
sxe_list_peek_head(SXE_LIST * list)
{
    SXE_LIST_NODE * node;
    void          * result;

    SXEE81("sxe_list_peek_head(list=%p)", list);

    if (list->HEAD == SENTINEL_PTR_REL(list)) {
        SXEA10(list->TAIL == SENTINEL_PTR_REL(list), "List head is the sentinel but tail is not the sentinel");
        result = NULL;
        goto SXE_ERROR_OUT;
    }

    SXEA10(list->TAIL                     != SENTINEL_PTR_REL(list), "List head is not the sentinel but tail is the sentinel");
    SXEA10(NODE_PTR_FIX(list->HEAD)->prev == SENTINEL_PTR_REL(list), "List's tail object is not the first object");
    node = NODE_PTR_FIX(list->HEAD);
    SXEA84(node->id == list->sentinel.id,                            "Node %p on list %p has id %u but list has %u", node, list,
           node->id, list->sentinel.id);

    /* Catch a bug seen in pool testing.
     */
    SXEA60(NODE_PTR_FIX(list->TAIL)->next == SENTINEL_PTR_REL(list), "List's tail object is not the last object");
    result = (char *)node - list->offset;

SXE_ERROR_OUT:
    SXER82("return object=%x%s", result, (result == 0) ? " // NULL" : "");
    return result;
}

/**
 * Peek at the object at the tail of a list
 */
void *
sxe_list_peek_tail(SXE_LIST * list)
{
    SXE_LIST_NODE * node;
    void          * result;

    SXEE81("sxe_list_peek_tail(list=%p)", list);

    if (list->TAIL == SENTINEL_PTR_REL(list)) {
        SXEA10(list->HEAD == SENTINEL_PTR_REL(list), "List tail is the sentinel but head is not the sentinel");
        result = NULL;
        goto SXE_ERROR_OUT;
    }

    SXEA10(list->HEAD                     != SENTINEL_PTR_REL(list), "List tail is not the sentinel but head is the sentinel");
    SXEA10(NODE_PTR_FIX(list->TAIL)->next == SENTINEL_PTR_REL(list), "List's head object is not the last object");
    node = NODE_PTR_FIX(list->TAIL);
    SXEA84(node->id == list->sentinel.id,                            "Node %p on list %p has id %u but list has %u", node, list,
           node->id, list->sentinel.id);

    /* Catch a bug seen in pool testing.
     */
    SXEA60(NODE_PTR_FIX(list->HEAD)->prev == SENTINEL_PTR_REL(list), "List's head object is not the first object");
    result = (char *)node - list->offset;

SXE_ERROR_OUT:
    SXER82("return object=%x%s", result, (result == 0) ? " // NULL" : "");
    return result;
}
