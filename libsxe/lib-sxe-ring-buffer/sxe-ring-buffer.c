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
#include <stdbool.h>
#include "sxe-ring-buffer-private.h"
#include "sxe-ring-buffer.h"
#include "sxe-log.h"
#include "sxe.h"

void *
sxe_ring_buffer_new(unsigned size)
{
    void * base;

    SXEE81("sxe_ring_buffer_new(size=%u)", size);
    SXEA10((base = malloc(size + SXE_RING_BUFFER_SIZE_OF_INTERNALS)) != NULL, "Error allocating sxe-ring-buffer");

    SXE_RING_BUFFER_SIZE       = size;
    SXE_RING_BUFFER_ITERATION  = 0;
    SXE_RING_BUFFER_CURRENT    = SXE_RING_BUFFER_ARRAY_BASE;
    SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_CURRENT;

    SXEL92("Array base=%p, End of Array=%p", SXE_RING_BUFFER_ARRAY_BASE, SXE_RING_BUFFER_END);
    SXER81("return base=%p", base);
    return base;
}

void sxe_ring_buffer_delete(void * base) { free(base); }

void
sxe_ring_buffer_add(void * base, const char * buf, unsigned len)
{
    unsigned available_space;

    SXEE82("sxe_ring_buffer_add(buf=%p,len=%u)", buf, len);
    SXEL92("Current: %p, Itteration: %u", SXE_RING_BUFFER_CURRENT, SXE_RING_BUFFER_ITERATION);

    SXEA10(len <= SXE_RING_BUFFER_SIZE, "The item we're will fit in the whole array");

    available_space = (SXE_RING_BUFFER_END - SXE_RING_BUFFER_CURRENT + 1);
    SXEL91("There is %u space between the current pointer and the end of the array", available_space);

    if (available_space >= len) { // Can we fit it on the end of the array?
        SXEL92("Adding %u bytes at %p", len, SXE_RING_BUFFER_CURRENT);
        memcpy(SXE_RING_BUFFER_CURRENT, buf, len);

        SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_CURRENT + len;
        if (SXE_RING_BUFFER_CURRENT == (SXE_RING_BUFFER_END + 1)) {
            SXEL90("Wrote to the last byte in the array, wrapping pointer");
            SXE_RING_BUFFER_ITERATION++;
            SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_ARRAY_BASE;
            SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_END;
        }
        else {
            if (SXE_RING_BUFFER_WRITEN_END < SXE_RING_BUFFER_CURRENT) {
                SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_CURRENT - 1;
            }
        }
    }
    else { // else we have to wrap around the ring
        memcpy(SXE_RING_BUFFER_CURRENT, buf, available_space);
        SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_END;
        SXE_RING_BUFFER_ITERATION++;
        SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_ARRAY_BASE;
        memcpy(SXE_RING_BUFFER_CURRENT, buf + available_space, (len - available_space));
        SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_CURRENT + (len - available_space);
    }

    SXEL93("Current: %p, Writen End %p, Itteration: %u", SXE_RING_BUFFER_CURRENT, SXE_RING_BUFFER_WRITEN_END, SXE_RING_BUFFER_ITERATION);
    SXER80("return");
}


void
sxe_ring_buffer_next_writable_block(void * base, SXE_RING_BUFFER_CONTEXT * context)
{
    SXEE80("sxe_ring_buffer_next_writable_block()");
    context->writable_block = SXE_RING_BUFFER_CURRENT;
    context->writable_block_len = SXE_RING_BUFFER_END - SXE_RING_BUFFER_CURRENT + 1;
    SXEA10(context->writable_block_len != 0, "The writable block length is not zero");
    SXEL92("Writable block: %p, Writable block length: %u", context->writable_block, context->writable_block_len);
    SXER80("return");
}


void
sxe_ring_buffer_wrote_block(void * base, SXE_RING_BUFFER_CONTEXT * context, unsigned len)
{
    SXEE81("sxe_ring_buffer_wrote_block(len=%u)", len);
    SXEA10(context->writable_block == SXE_RING_BUFFER_CURRENT, "The current pointer is still where it was when you asked for it");
    SXEA10(len <= (unsigned)(SXE_RING_BUFFER_END - SXE_RING_BUFFER_CURRENT + 1), "Did not overwrite end of buffer ring");

    SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_CURRENT + len;

    if (SXE_RING_BUFFER_CURRENT == (SXE_RING_BUFFER_END + 1)) {
        SXEL90("Wrote to the last byte in the array, wrapping pointer");
        SXE_RING_BUFFER_ITERATION++;
        SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_ARRAY_BASE;
        SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_END;
    }
    else {
        if (SXE_RING_BUFFER_WRITEN_END < SXE_RING_BUFFER_CURRENT) {
            SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_CURRENT - 1;
        }
    }

    SXEL93("Current: %p, Writen End %p, Itteration: %u", SXE_RING_BUFFER_CURRENT, SXE_RING_BUFFER_WRITEN_END, SXE_RING_BUFFER_ITERATION);
    SXER80("return");
}


void
sxe_ring_buffer_force_ring_wrap(void * base)
{
    SXEE80("sxe_ring_buffer_force_ring_wrap()");
    SXE_RING_BUFFER_WRITEN_END = SXE_RING_BUFFER_CURRENT - 1;
    SXE_RING_BUFFER_CURRENT = SXE_RING_BUFFER_ARRAY_BASE;
    SXE_RING_BUFFER_ITERATION++;
    SXER80("return");
}

void
sxe_ring_buffer_next_writable_block_size(void * base, SXE_RING_BUFFER_CONTEXT * context, unsigned size)
{
    SXEE81("sxe_ring_buffer_next_writable_block_size(size=%u)", size);
    SXEA10(size <= SXE_RING_BUFFER_SIZE, "The requested size is equall or smaller then the ring");
    sxe_ring_buffer_next_writable_block(base, context);
    if (context->writable_block_len < size) {
        sxe_ring_buffer_force_ring_wrap(base);
        sxe_ring_buffer_next_writable_block(base, context);
    }
    SXER80("return");
}

void
sxe_ring_buffer_join(void * base, SXE_RING_BUFFER_CONTEXT * context, bool replay_half)
{
    SXEE80("sxe_ring_buffer_join()");
    context->itteration     = SXE_RING_BUFFER_ITERATION;
    context->data_block     = SXE_RING_BUFFER_CURRENT;
    context->data_block_len = 0;

    if (replay_half) {
        if (SXE_RING_BUFFER_CURRENT - SXE_RING_BUFFER_ARRAY_BASE >= (int)(SXE_RING_BUFFER_SIZE / 2)) {
            // If the current pointer is over half way through
            SXEL90("Current pointer is more then half way through ring");
            context->data_block = SXE_RING_BUFFER_CURRENT - (unsigned)(SXE_RING_BUFFER_SIZE / 2);
        }
        else if (SXE_RING_BUFFER_ITERATION == 0) {
            // First iteration, and havn't writen a half ring yet, go to the start of the array
            context->data_block = SXE_RING_BUFFER_ARRAY_BASE;
        }
        else {
            // else we have to go back an itteration
            SXEL90("Current pointer is less then half way through ring");
            SXEL90("Going back an itteration");
            context->itteration--;
            context->data_block = SXE_RING_BUFFER_CURRENT + (int)((SXE_RING_BUFFER_WRITEN_END - SXE_RING_BUFFER_ARRAY_BASE + 1) / 2);
        }
    }
    SXEL92("context data_block: %p, itteration: %u", context->data_block, context->itteration);
    SXER80("return");
}


static SXE_RETURN
sxe_ring_buffer_check_over_run(void * base, SXE_RING_BUFFER_CONTEXT * context)
{
    SXE_RETURN result = SXE_RETURN_OK;
    SXEE80("sxe_ring_buffer_check_over_run()");
    SXEL92("ring context data_block: %p, itteration: %u", SXE_RING_BUFFER_CURRENT, SXE_RING_BUFFER_ITERATION);
    SXEL92("user context data_block: %p, itteration: %u", context->data_block, context->itteration);

    if  ( (SXE_RING_BUFFER_ITERATION > (context->itteration + 1))
        || ((SXE_RING_BUFFER_ITERATION > context->itteration) && (SXE_RING_BUFFER_CURRENT > context->data_block)))
    {
       SXEL90("This reader has fallen behind the writer!");
       result = SXE_RETURN_ERROR_INTERNAL;
       SXEL90("Moving data_block to the start of the ring");
       context->data_block = SXE_RING_BUFFER_CURRENT;
       context->itteration = SXE_RING_BUFFER_ITERATION;
       context->data_block_len = 0;
    }

    SXER81("return %s", sxe_return_to_string(result));
    return result;
}


SXE_RETURN
sxe_ring_buffer_next_block(void * base, SXE_RING_BUFFER_CONTEXT * context)
{
    SXE_RETURN result = SXE_RETURN_OK;
    SXEE80("sxe_ring_buffer_next_block()");

    if ((result = sxe_ring_buffer_check_over_run(base, context)) == SXE_RETURN_ERROR_INTERNAL) {
        goto SXE_ERROR_OUT;
    }

    if (SXE_RING_BUFFER_CURRENT > context->data_block) {
        SXEL90("Reading up to the head of the ring");
        context->data_block_len = SXE_RING_BUFFER_CURRENT - context->data_block;
    }
    else if (SXE_RING_BUFFER_CURRENT <= context->data_block &&
             SXE_RING_BUFFER_ITERATION == context->itteration + 1 &&
             SXE_RING_BUFFER_WRITEN_END == context->data_block - 1) {
        SXEL90("The ring was forced wrapped while context was at the head");
        SXEL90("Wrap the ring and read up to the head");
        context->data_block = SXE_RING_BUFFER_ARRAY_BASE;
        context->itteration++;
        context->data_block_len = SXE_RING_BUFFER_CURRENT - context->data_block;
    }
    else if (SXE_RING_BUFFER_ITERATION > context->itteration) {
        SXEL90("Reading to the end of the ring");
        context->data_block_len = SXE_RING_BUFFER_WRITEN_END - context->data_block + 1;
    }
    else {
        SXEL90("Nothing to read");
    }

SXE_EARLY_OR_ERROR_OUT:
    SXEL93("context data_block: %p, data_block_len: %u itteration: %u", context->data_block, context->data_block_len, context->itteration);
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}


SXE_RETURN
sxe_ring_buffer_consumed(void * base, SXE_RING_BUFFER_CONTEXT * context, unsigned len)
{
    SXE_RETURN result = SXE_RETURN_OK;
    SXEE81("sxe_ring_buffer_consumed(len:%u)", len);

    if ((result = sxe_ring_buffer_check_over_run(base, context)) == SXE_RETURN_ERROR_INTERNAL) {
        goto SXE_ERROR_OUT;
    }

    SXEL92("data_block was: %p, moving to %p", context->data_block, context->data_block + len);
    context->data_block     += len;
    context->data_block_len -= len;

    SXEA10(context->data_block <= SXE_RING_BUFFER_WRITEN_END + 1, "the data block did not go off the end of the array");

    if ((context->data_block == SXE_RING_BUFFER_WRITEN_END + 1) && (context->data_block != SXE_RING_BUFFER_CURRENT)) {
        SXEA10(context->data_block_len == 0, "If this consume brought us to the end of the array, the remaining length must be zero");
        SXEL90("Wrapping data_block to the start of the ring");
        context->data_block = SXE_RING_BUFFER_ARRAY_BASE;
        context->itteration++;
    }

SXE_EARLY_OR_ERROR_OUT:
    SXEL93("context data_block: %p, data_block_len: %u itteration: %u", context->data_block, context->data_block_len, context->itteration);
    SXER81("return %s", sxe_return_to_string(result));
    return result;
}

