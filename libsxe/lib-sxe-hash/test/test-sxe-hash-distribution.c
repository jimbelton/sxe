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

#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "sxe.h"
#include "sxe-hash.h"
#include "sxe-log.h"
#include "sxe-pool.h"
#include "tap.h"
#include "sha1sum.h"

#define HASH_SIZE                       10000
#define MAX_BUCKET_INDEX                (HASH_SIZE + 1)
#define MAX_ALLOWED_PER_BUCKET_INDEX    7

int main(void)
{
    SXE_HASH * hash;
    int i;
    int counter[MAX_BUCKET_INDEX];

    plan_tests(2 * HASH_SIZE);
    memset(counter, 0, MAX_BUCKET_INDEX * sizeof(int));
    hash = sxe_hash_new("test-hash", HASH_SIZE, sizeof(SXE_HASH_KEY_VALUE_PAIR), SXE_HASH_SHA1_AS_CHAR_LENGTH, 0);
    SXEA10(hash != NULL, "Couldn't create hash 'test-hash'");

    for (i = 0; i < HASH_SIZE; i++)
    {
        unsigned id;
        char in_buf[5];
        char out_buf[41];

        snprintf(in_buf, 5, "%d", i);
        sha1sum(in_buf, 4, out_buf);

        id = sxe_hash_set(hash, out_buf, SXE_HASH_SHA1_AS_CHAR_LENGTH, 1U);
        id = sxe_pool_index_to_state(hash->pool, id);

        ok(id > 0 && id < MAX_BUCKET_INDEX, "bucket index: %u is within range", id);

        counter[id]++;
        ok(counter[id] < MAX_ALLOWED_PER_BUCKET_INDEX, "count at bucket index: %u is less than %d", counter[id], MAX_ALLOWED_PER_BUCKET_INDEX);
    }

    return exit_status();
}
