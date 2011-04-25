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

#include <stddef.h>
#include <string.h>

#include "sxe-list.h"
#include "sxe-log.h"
#include "tap.h"

static struct blob {
    SXE_LIST list;
    struct list_obj {
        unsigned      id;
        SXE_LIST_NODE node;
    } list_obj[4];
} blob;

int
main(void)
{
    unsigned          i;
    struct list_obj * obj_ptr;
    struct blob     * blob_copy;
    struct blob     * blob_ptr;
    SXE_LIST_WALKER   walker;
    struct list_obj * last_obj_ptr = NULL;    /* STFU gcc */

    for (i = 0; i < 4; i++) {
        blob.list_obj[i].id = i;
    }

    plan_tests(46);

    SXE_LIST_CONSTRUCT(&blob.list, 1, struct list_obj, node);
    is(SXE_LIST_GET_LENGTH(&blob.list), 0,                               "List has 0 elements");
    is(sxe_list_peek_head( &blob.list), NULL,                            "Can't peek at head of empty list");
    is(sxe_list_peek_tail( &blob.list), NULL,                            "Can't peek at tail of empty list");

    sxe_list_push(&blob.list, &blob.list_obj[0]);
    obj_ptr = sxe_list_pop(&blob.list);
    ok(obj_ptr != NULL,                                                  "Popped an object back from list");
    is(obj_ptr->id,                     0,                               "Popped object 0 back from list");
    obj_ptr = sxe_list_pop(&blob.list);
    ok(obj_ptr == NULL,                                                  "Pop detected that list is empty");

    sxe_list_push(&blob.list,    &blob.list_obj[0]);
    sxe_list_unshift(&blob.list, &blob.list_obj[1]);
    sxe_list_push(&blob.list,    &blob.list_obj[2]);
    sxe_list_unshift(&blob.list, &blob.list_obj[3]);                     /* List now contains: 3, 1, 0, 2 */

    SXEA10((blob_copy = malloc(sizeof(blob))) != NULL, "Couldn't allocate memory for the list and all the objects");
    memcpy(blob_copy, &blob, sizeof(blob));

    for (blob_ptr = &blob; blob_ptr != NULL; blob_ptr = (blob_ptr == &blob ? blob_copy : NULL)) {
        is(sxe_list_peek_head(&blob_ptr->list),  &blob_ptr->list_obj[3], "Peek at object 3 on head of list");
        is(sxe_list_peek_tail(&blob_ptr->list),  &blob_ptr->list_obj[2], "Peek at object 2 on tail of list");
        is(SXE_LIST_GET_LENGTH(&blob_ptr->list), 4,                      "List has 4 elements");
        sxe_list_walker_construct(&walker, &blob_ptr->list);

        for (i = 0; (obj_ptr = (struct list_obj *)sxe_list_walker_step(&walker)) != NULL; i++) {
            is(obj_ptr, (struct list_obj *)sxe_list_walker_find(&walker), "Walker find agrees with next");

            if (obj_ptr->id == 1) {
                i++;
                break;
            }
        }

        ok(obj_ptr                            != NULL,                   "Found an element with id == 1");
        is(i,                                    2,                      "Two elements visited");

        sxe_list_walker_construct(&walker, &blob_ptr->list);

        for (i = 0; (obj_ptr = (struct list_obj *)sxe_list_walker_step(&walker)) != NULL; i++) {
            last_obj_ptr = obj_ptr;

            if (obj_ptr->id == 4) {
                i++;
                break;
            }
        }

        ok(obj_ptr                  == NULL,                             "Found no element with id == 4");
        is(i,                          4,                                "All four elements visited");
        obj_ptr = sxe_list_walker_back(&walker);
        is(obj_ptr->id, last_obj_ptr->id,                                "Backed up to element %u is last element %u",
           obj_ptr->id, last_obj_ptr->id);
        obj_ptr = sxe_list_remove(&blob_ptr->list, &blob_ptr->list_obj[1]);
        is(obj_ptr,                    &blob_ptr->list_obj[1],           "Removed object 1");
        obj_ptr = sxe_list_shift(&blob_ptr->list);
        ok(obj_ptr != NULL,                                              "Shifted an object from the list");
        is(obj_ptr->id,                3,                                "Shifted object 3 from the list");
        obj_ptr = sxe_list_pop(&blob_ptr->list);
        ok(obj_ptr != NULL,                                              "Popped another object back from list");
        is(obj_ptr->id,                2,                                "Popped object 2 back from list");
        obj_ptr = sxe_list_shift(&blob_ptr->list);
        ok(obj_ptr != NULL,                                              "Shifted the last object from the list");
        is(obj_ptr->id,                0,                                "Shifted object 0 from the list");
        obj_ptr = sxe_list_shift(&blob_ptr->list);
        ok(obj_ptr == NULL,                                              "Shift detected that list is empty");

        sxe_list_unshift(&blob_ptr->list, &blob_ptr->list_obj[1]);
        sxe_list_push(&blob_ptr->list,    &blob_ptr->list_obj[3]);
        sxe_list_remove(&blob_ptr->list,  &blob_ptr->list_obj[3]);
        sxe_list_unshift(&blob_ptr->list, &blob_ptr->list_obj[3]);
        sxe_list_remove(&blob_ptr->list,  &blob_ptr->list_obj[3]);

        obj_ptr = sxe_list_shift(&blob_ptr->list);
        ok(obj_ptr != NULL,                                              "Shifted the last object from the list");
        is(obj_ptr->id, 1,                                               "Shifted object 1 from the list");

        /* Try to reproduce problem found while developing pools.
        */
        SXE_LIST_CONSTRUCT(&blob_ptr->list, 2, struct list_obj, node);
        sxe_list_push(&blob_ptr->list,   &blob_ptr->list_obj[0]);
        sxe_list_push(&blob_ptr->list,   &blob_ptr->list_obj[1]);
        sxe_list_remove(&blob_ptr->list, &blob_ptr->list_obj[0]);
        sxe_list_push(&blob_ptr->list,   &blob_ptr->list_obj[0]);

        memset(blob_ptr, 0xBE, sizeof(blob));
    }

    return exit_status();
}
