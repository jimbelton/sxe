/* Copyright (c) 2009, Jim Belton. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "tap.h"

struct tap_ev {
    const void *    identifier;
    unsigned        argc;
    void **         argv;
    struct tap_ev * next;
};

struct tap_ev_queue {
    unsigned queue_count;
    tap_ev   head;
    tap_ev   tail;
};

/* Constant string declared as a global to allow it's address to be used.
 */
const char   TAP_EV_NO_EVENT[] = "NO EVENT";

static struct tap_ev_queue tap_ev_queue_default = {0, NULL, NULL};

const void * tap_ev_identifier(tap_ev ev) {return ev == NULL ? TAP_EV_NO_EVENT : ev->identifier;}
unsigned     tap_ev_arg_count( tap_ev ev) {return ev->argc;                                     }

const void *
tap_ev_arg(tap_ev ev, const char * name)
{
    unsigned i;

    for (i = 0; i < ev->argc; i++) {
        if (strcmp(ev->argv[i * 2    ], name) == 0) {
            return ev->argv[i * 2 + 1];
        }
    }

    fprintf(stderr, "tap_ev_arg: '%s' event has no argument '%s'", (const char *)tap_ev_identifier(ev), name);
    assert("Argument was not found" == NULL);
    return NULL;    /* Can't happen */
}

void
tap_ev_free(tap_ev ev)
{
    assert(ev->argv == (void **)(ev + 1));
    ev->argv = NULL;
    free(ev);
}

static void
tap_ev_queue_push_vararg(tap_ev_queue queue, const char * identifier, unsigned argc, va_list ap)
{
    tap_ev   ev;
    unsigned i;

    ev = malloc(sizeof(struct tap_ev) + 2 * argc * sizeof(void *));
    assert(ev != NULL);
    ev->identifier = identifier;
    ev->argc       = argc;
    ev->argv       = (void **)(ev + 1);
    ev->next       = NULL;

    for (i = 0; i < argc; i++) {
        ev->argv[i * 2    ] = va_arg(ap, char *);
        ev->argv[i * 2 + 1] = va_arg(ap, void *);
    }

    queue->queue_count++;
    assert(queue->queue_count != 0);

    if (queue->head == NULL) {
        queue->head = ev;
        queue->tail = ev;
    }
    else {
        queue->tail->next = ev;
        queue->tail       = ev;
    }
}

void
tap_ev_queue_push(tap_ev_queue queue, const char * identifier, unsigned argc, ...)
{
    va_list ap;

    va_start(ap, argc);
    tap_ev_queue_push_vararg(queue, identifier, argc, ap);
    va_end(ap);
}

void
tap_ev_push(const char * identifier, unsigned argc, ...)
{
    va_list ap;

    va_start(ap, argc);
    tap_ev_queue_push_vararg(&tap_ev_queue_default, identifier, argc, ap);
    va_end(ap);
}

unsigned
tap_ev_queue_count(tap_ev_queue queue, const char * identifier)
{
    tap_ev   ev;
    unsigned count = 0;

    for (ev = queue->head; ev != NULL; ev = ev->next) {
        if (strcmp(ev->identifier, identifier) == 0) {
            count++;
        }
    }

    return count;
}

unsigned
tap_ev_count(const char * identifier)
{
    return tap_ev_queue_count(&tap_ev_queue_default, identifier);
}

unsigned
tap_ev_queue_length(tap_ev_queue queue)
{
    return queue->queue_count;
}

unsigned
tap_ev_length(void)
{
    return tap_ev_queue_default.queue_count;
}

tap_ev
tap_ev_queue_shift(tap_ev_queue queue)
{
    tap_ev ev = queue->head;

    if (ev == NULL) {
        assert(queue->queue_count == 0);
        return NULL;
    }

    queue->head = ev->next;
    assert(queue->queue_count != 0);
    queue->queue_count--;
    return ev;
}

tap_ev
tap_ev_shift(void)
{
    return tap_ev_queue_shift(&tap_ev_queue_default);
}

tap_ev
tap_ev_queue_shift_next(tap_ev_queue queue, const char * identifier)
{
    tap_ev  ev = queue->head;
    tap_ev  prev;

    if (ev == NULL) {
        assert(queue->queue_count == 0);
        return NULL;
    }

    if (strcmp(ev->identifier, identifier) == 0) {
        return tap_ev_queue_shift(queue);
    }

    for (prev = ev, ev = ev->next; ev != NULL; prev = ev, ev = ev->next) {
        if (strcmp(ev->identifier, identifier) == 0) {
            prev->next = ev->next;
            ev->next = NULL;
            assert(queue->queue_count != 0);
            queue->queue_count--;
            return ev;
        }
    }

    return NULL;
}

tap_ev
tap_ev_shift_next(const char * identifier)
{
    return tap_ev_queue_shift_next(&tap_ev_queue_default, identifier);
}

void
tap_ev_queue_flush(tap_ev_queue queue)
{
    tap_ev ev;

    while ((ev = tap_ev_queue_shift(queue)) != NULL) {
        tap_ev_free(ev);
    }
}

void
tap_ev_flush(void)
{
    tap_ev_queue_flush(&tap_ev_queue_default);
}

tap_ev_queue
tap_ev_queue_get_default(void)
{
    return &tap_ev_queue_default;
}

tap_ev_queue
tap_ev_queue_new(void)
{
    tap_ev_queue queue;

    assert((queue = calloc(1, sizeof(struct tap_ev_queue))) != NULL && "tap_ev_queue_new failed");
    return queue;
}
