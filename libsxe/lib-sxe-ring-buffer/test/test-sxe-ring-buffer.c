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

#include <assert.h>
#include <string.h>

#include "sxe-ring-buffer.h"
#include "sxe-ring-buffer-private.h"
#include "sxe-log.h"
#include "tap.h"

#define LOG_BUFFER_DETAILS 1

static void
validate_ring_buffer_array   (SXE_RING_BUFFER base, const char * buf, unsigned len, unsigned line_no)
{
    unsigned i;

    ok(memcmp(SXE_RING_BUFFER_ARRAY_BASE, buf, len) == 0, "ring buffer array is as expected: line %u", line_no);

    if (LOG_BUFFER_DETAILS) {
        for (i = 0; i < len; i++) {
            SXEL6("[%u]='%c', expected '%c'", i, *(SXE_RING_BUFFER_ARRAY_BASE + i), buf[i]);
        }
    }
}

static void
test_ring_buffer_add_itterations(void)
{
    SXE_RING_BUFFER ring_buf;

    ring_buf = sxe_ring_buffer_new(10);

    ok(ring_buf != NULL, "Allocated a new ring buffer");

    sxe_ring_buffer_add       (ring_buf,     "abcdefg",  7);
    validate_ring_buffer_array(ring_buf,     "abcdefg",  7,  __LINE__);

    sxe_ring_buffer_add       (ring_buf,         "hij",  3);
    validate_ring_buffer_array(ring_buf,  "abcdefghij", 10,  __LINE__);

    sxe_ring_buffer_add       (ring_buf,          "kl",  2);
    validate_ring_buffer_array(ring_buf,  "klcdefghij", 10,  __LINE__);

    sxe_ring_buffer_add       (ring_buf,          "mn",  2);
    validate_ring_buffer_array(ring_buf,  "klmnefghij", 10,  __LINE__);

    sxe_ring_buffer_add       (ring_buf,   "opqrstuvw",  9);
    validate_ring_buffer_array(ring_buf,  "uvwnopqrst", 10,  __LINE__);

    sxe_ring_buffer_delete(ring_buf);
}

static void
validate_ring_buffer_read(SXE_RING_BUFFER_CONTEXT * context, const char * buf, unsigned line_no)
{
    is_strncmp(SXE_RING_BUFFER_BLOCK(context), buf, strlen(buf), "ring buffer read - data as expected: line %u", line_no);
    is(SXE_RING_BUFFER_BLOCK_LENGTH(context), strlen(buf), "ring buffer read - length as expected: line %u", line_no);
}

static void
test_ring_buffer_join_and_read(void)
{
    SXE_RING_BUFFER         ring_buf;
    SXE_RING_BUFFER_CONTEXT context;

    ring_buf = sxe_ring_buffer_new(10);
    ok(ring_buf != NULL, "Allocated a new ring buffer");

    sxe_ring_buffer_add          (ring_buf,    "abcdefg",   7);
    validate_ring_buffer_array   (ring_buf,    "abcdefg",   7,   __LINE__);

    sxe_ring_buffer_join         (ring_buf,     &context,   0);

    // basic add followed by a read
    ok(sxe_ring_buffer_next_block(ring_buf,     &context)        == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,    "abcdefg",   7,   __LINE__);
    validate_ring_buffer_read    (&context,           "",        __LINE__);

    // 2 adds then an read
    sxe_ring_buffer_add          (ring_buf,          "h",   1);
    sxe_ring_buffer_add          (ring_buf,          "i",   1);
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)       == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,   "abcdefghi",  9,   __LINE__);
    validate_ring_buffer_read    (&context,          "hi",       __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,      &context,  2)   == SXE_RETURN_OK, "Consumed was successful");

    // an add to the end of the ring, followed by 2 reads
    sxe_ring_buffer_add          (ring_buf,           "j",  1);
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)       == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "abcdefghij", 10,   __LINE__);
    validate_ring_buffer_read    (&context,           "j",       __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,      &context,  1)   == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)       == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "abcdefghij", 10,   __LINE__);
    validate_ring_buffer_read    (&context,            "",       __LINE__);

    // an add to the start of the ring (next itteration), followed by 2 reads
    sxe_ring_buffer_add          (ring_buf,         "klm",  3);
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "klmdefghij", 10,  __LINE__);
    validate_ring_buffer_read    (&context,         "klm",      __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,      &context,  3)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "klmdefghij", 10,  __LINE__);
    validate_ring_buffer_read    (&context,            "",      __LINE__);

    // an add that wraps the whole ring, then 3 reads
    sxe_ring_buffer_add          (ring_buf,   "nopqrstuv",  9);
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)  == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "uvmnopqrst", 10,  __LINE__);
    validate_ring_buffer_read    (&context,     "nopqrst",      __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,      &context,  7)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)  == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "uvmnopqrst", 10,  __LINE__);
    validate_ring_buffer_read    (&context,          "uv",      __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,      &context,  2)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,      &context)  == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_array   (ring_buf,  "uvmnopqrst", 10,  __LINE__);
    validate_ring_buffer_read    (&context,            "",      __LINE__);

    sxe_ring_buffer_delete       (ring_buf);
}

static void
test_ring_buffer_next_and_consumed_fails(void)
{
    SXE_RING_BUFFER         ring_buf;
    SXE_RING_BUFFER_CONTEXT context;

    ring_buf = sxe_ring_buffer_new(10);
    ok(ring_buf != NULL, "Allocated a new ring buffer");

    sxe_ring_buffer_join(ring_buf, &context, 0);

    // read fail because the writer has "lapped" the reader
    sxe_ring_buffer_add          (ring_buf,    "abcdefghij",   10);
    validate_ring_buffer_array   (ring_buf,    "abcdefghij",   10,   __LINE__);
    sxe_ring_buffer_add          (ring_buf,             "k",   1);
    validate_ring_buffer_array   (ring_buf,    "kbcdefghij",   10,   __LINE__);

    ok(sxe_ring_buffer_next_block(ring_buf,        &context)   ==   SXE_RETURN_ERROR_INTERNAL, "Read was successful");
    validate_ring_buffer_array   (ring_buf,    "kbcdefghij",   10,   __LINE__);
    validate_ring_buffer_read    (&context,              "",         __LINE__);

    // read fail because the writer is more then 1 itteration ahead
    sxe_ring_buffer_add          (ring_buf,    "lmnopqrstu",   10);
    validate_ring_buffer_array   (ring_buf,    "ulmnopqrst",   10,   __LINE__);
    sxe_ring_buffer_add          (ring_buf,    "vwxyzabcde",   10);
    validate_ring_buffer_array   (ring_buf,    "evwxyzabcd",   10,   __LINE__);

    ok(sxe_ring_buffer_next_block(ring_buf,        &context)         == SXE_RETURN_ERROR_INTERNAL, "Read was unsuccessful");
    validate_ring_buffer_array   (ring_buf,    "evwxyzabcd",   10,   __LINE__);
    validate_ring_buffer_read    (&context,              "",         __LINE__);

    // read fails because the writer wrote into the block the reader was still reading
    sxe_ring_buffer_add          (ring_buf,    "fghijklmno",   10);
    validate_ring_buffer_array   (ring_buf,    "ofghijklmn",   10,   __LINE__);
    ok(sxe_ring_buffer_next_block(ring_buf,        &context)         == SXE_RETURN_OK, "Read was successful");

    validate_ring_buffer_read    (&context,     "fghijklmn",         __LINE__);
    sxe_ring_buffer_add          (ring_buf,          "pqrs",    4);
    validate_ring_buffer_array   (ring_buf,    "opqrsjklmn",   10,   __LINE__);

    ok(sxe_ring_buffer_next_block(ring_buf,        &context)         == SXE_RETURN_ERROR_INTERNAL, "Read was unsuccessful");
    validate_ring_buffer_read    (&context,              "",         __LINE__);

    // consume fails
    sxe_ring_buffer_add          (ring_buf,    "tuvwxyzabc",   10);
    validate_ring_buffer_array   (ring_buf,    "yzabctuvwx",   10,   __LINE__);
    ok(sxe_ring_buffer_next_block(ring_buf,        &context)         == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,         "tuvwx",         __LINE__);
    sxe_ring_buffer_add          (ring_buf,    "defghijklm",   10);
    validate_ring_buffer_array   (ring_buf,    "ijklmdefgh",   10,   __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  1)         == SXE_RETURN_ERROR_INTERNAL, "Consumed returned error");

    ok(sxe_ring_buffer_next_block(ring_buf,        &context)         == SXE_RETURN_OK, "Read was unsuccessful");
    validate_ring_buffer_read    (&context,              "",         __LINE__);
}

static void
test_ring_buffer_join_replay_half(void)
{
    SXE_RING_BUFFER         ring_buf;
    SXE_RING_BUFFER_CONTEXT context;

    ring_buf = sxe_ring_buffer_new(8);

    // join a ring that's empty
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);

    // join a ring that has 1 byte in it
    sxe_ring_buffer_add          (ring_buf,         "a",  1);
    validate_ring_buffer_array   (ring_buf,         "a",  1,   __LINE__);
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,         "a",                         __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  1)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);

    // join a ring that has 2 bytes in it
    sxe_ring_buffer_add          (ring_buf,         "b",  1);
    validate_ring_buffer_array   (ring_buf,        "ab",  2,                    __LINE__);
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,        "ab",                        __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  2)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);

    // join a ring that is half full
    sxe_ring_buffer_add          (ring_buf,        "cd",  2);
    validate_ring_buffer_array   (ring_buf,      "abcd",  4,                    __LINE__);
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,      "abcd",                        __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  4)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);

    // join a ring that is full
    sxe_ring_buffer_add          (ring_buf,      "efgh",  4);
    validate_ring_buffer_array   (ring_buf,  "abcdefgh",  8,                    __LINE__);
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,      "efgh",                        __LINE__);

    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  4)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);

    // join a ring that is 1.25 times full
    sxe_ring_buffer_add          (ring_buf,        "ij",  2);
    validate_ring_buffer_array   (ring_buf,  "ijcdefgh",  8,                    __LINE__);
    sxe_ring_buffer_join         (ring_buf,    &context,  1);
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,        "gh",                        __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  2)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,        "ij",                        __LINE__);
    ok(sxe_ring_buffer_consumed  (ring_buf,    &context,  2)  == SXE_RETURN_OK, "Consumed was successful");
    ok(sxe_ring_buffer_next_block(ring_buf,    &context)      == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read    (&context,          "",                        __LINE__);
}


static void
validate_ring_buffer_next_writable(SXE_RING_BUFFER_CONTEXT * context, unsigned len, unsigned line_no)
{
    is(SXE_RING_BUFFER_WRITABLE_BLOCK_LENGTH(context), len, "ring buffer next writable block - length is correct: line %u", line_no);
}

static void
write_to_ring_buffer(SXE_RING_BUFFER_CONTEXT * context, const char * buf)
{
    memcpy(SXE_RING_BUFFER_WRITABLE_BLOCK(context), buf, strlen(buf));
}

static void
test_ring_buffer_next_writable_and_force_wrap(void)
{
    SXE_RING_BUFFER         ring_buf;
    SXE_RING_BUFFER_CONTEXT context;

    ring_buf = sxe_ring_buffer_new(8);

    // Write 4 bytes to the ring
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          8,   __LINE__);
    write_to_ring_buffer               (&context,     "abcd");
    sxe_ring_buffer_wrote_block        (ring_buf,   &context,          4);
    validate_ring_buffer_array         (ring_buf,     "abcd",          4,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          4,   __LINE__);

    // Write 3 more bytes
    write_to_ring_buffer               (&context,      "efg");
    sxe_ring_buffer_wrote_block        (ring_buf,   &context,          3);
    validate_ring_buffer_array         (ring_buf,  "abcdefg",          7,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          1,   __LINE__);

    // Write 1 more bytes (to cause a wrap)
    write_to_ring_buffer               (&context,        "h");
    sxe_ring_buffer_wrote_block        (ring_buf,   &context,          1);
    validate_ring_buffer_array         (ring_buf, "abcdefgh",          8,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          8,   __LINE__);

    // write 7 more bytes and then force a wrap
    write_to_ring_buffer               (&context,    "ijklmn");
    sxe_ring_buffer_wrote_block        (ring_buf,   &context,          6);
    validate_ring_buffer_array         (ring_buf,  "ijklmngh",         8,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          2,   __LINE__);
    sxe_ring_buffer_force_ring_wrap    (ring_buf);
    validate_ring_buffer_array         (ring_buf,  "ijklmngh",          8,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          8,   __LINE__);

    // write 2 bytes after a forced ring wrap
    write_to_ring_buffer               (&context,        "op");
    sxe_ring_buffer_wrote_block        (ring_buf,   &context,          2);
    validate_ring_buffer_array         (ring_buf,  "opklmngh",         8,   __LINE__);
    sxe_ring_buffer_next_writable_block(ring_buf,   &context);
    validate_ring_buffer_next_writable (&context,          6,   __LINE__);

    // join a ring with half replay when it has had a 'force ring wrap'
    sxe_ring_buffer_join               (ring_buf,    &context,         1);
    ok(sxe_ring_buffer_next_block      (ring_buf,    &context)             == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read          (&context,         "n",                               __LINE__);
    ok(sxe_ring_buffer_consumed        (ring_buf,    &context,         1)  == SXE_RETURN_OK, "Consumed was successful");

    ok(sxe_ring_buffer_next_block      (ring_buf,    &context)             == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read          (&context,        "op",                               __LINE__);
    ok(sxe_ring_buffer_consumed        (ring_buf,    &context,         2)  == SXE_RETURN_OK, "Consumed was successful");

    // a writable_block_size forces a ring wrap with the reader context at the head of the ring
    sxe_ring_buffer_next_writable_block_size(ring_buf,    &context,    7);
    validate_ring_buffer_next_writable      (&context,           8,        __LINE__);
    write_to_ring_buffer                    (&context,        "q");
    sxe_ring_buffer_wrote_block             (ring_buf,    &context,    1);
    validate_ring_buffer_array              (ring_buf,  "qpklmngh",    8,  __LINE__);

    ok(sxe_ring_buffer_next_block           (ring_buf,    &context)        == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read               (&context,         "q",        __LINE__);
    ok(sxe_ring_buffer_consumed             (ring_buf,    &context,    1)  == SXE_RETURN_OK, "Consumed was successful");

    ok(sxe_ring_buffer_next_block           (ring_buf,    &context)        == SXE_RETURN_OK, "Read was successful");
    validate_ring_buffer_read               (&context,          "",        __LINE__);
}




int
main(void)
{
    plan_tests(156);
    test_ring_buffer_add_itterations();
    test_ring_buffer_join_and_read();
    test_ring_buffer_next_and_consumed_fails();
    test_ring_buffer_join_replay_half();
    test_ring_buffer_next_writable_and_force_wrap();
    return exit_status();
}

