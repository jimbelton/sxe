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

#include <stdio.h>

#include "sha1.h"
#include "sxe-hash.h"
#include "sxe-time.h"

int
main(int argc, char * argv[])
{
    SOPHOS_SHA1 * hash;
    SXE_TIME      start_time;
    unsigned      i;
    unsigned      id;
    char          key[10];

    (void)argv;

    if (argc == 1) {
        fprintf(stderr, "To benchmark hash, run: build-linux-32-release/test-sxe-hash-bench with options:\n");
        fprintf(stderr, "    -s = sha1 of 8 byte keys; -l = lookup3 of 8 byte keys\n");
        exit(0);
    }

    hash = sxe_hash_new_plus("testhash", 1 << 16, sizeof(SXE_SHA1), 0, sizeof(SXE_SHA1), SXE_HASH_OPTION_UNLOCKED);
    start_time = sxe_time_get();

    for (i = 0; sxe_time_get() - start_time < sxe_time_from_unix_time(1); i++) {
        if ((id = sxe_hash_take(hash)) == SXE_HASH_FULL) {
            i = i *  (sxe_time_get() - start_time) / sxe_time_from_unix_time(1);
            break;
        }

        snprintf(key, sizeof(key), "%08x", i);
        sophos_sha1(key, 8, (char *)&hash[id]);
        sxe_hash_add(hash, id);
    }

#if TODO
    hash = sxe_hash_new_plus("testhash", 1 << 16, sizeof(SXE_SHA1), 0, sizeof(SXE_SHA1), SXE_HASH_OPTION_UNLOCKED);
    start_time = sxe_time_get();

    for (i = 0; sxe_time_get() - start_time < sxe_time_from_unix_time(1); i++) {
        if ((id = sxe_hash_take(hash)) == SXE_HASH_FULL) {
            i = i *  (sxe_time_get() - start_time) / sxe_time_from_unix_time(1);
            break;
        }

        snprintf(key, sizeof(key), "%08x", i);
        sophos_sha1(key, 8, (char *)&hash[id]);
        sxe_hash_add(hash, id);
    }
#endif

    printf("SHA1: hashed %u 8 byte keys in 1 second\n", i);
    return 0;
}
