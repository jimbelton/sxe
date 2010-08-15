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

#include "sxe-hash.h"
#include "sxe-log.h"
#include "tap.h"

#define HASH_SIZE  5

// These two sha_keys will map to the same int_key
#define SHA1_1ST "2ce679528627da7780f8a4fec07cb34f902468a0"
#define SHA1_2ND "2ce679528627da7780f8a4fec07cb34f902464b8"

#define SHA1_3RD "2ce679528627da7780f8a4fec07cb34f902464b7"
#define SHA1_4TH "2ce679528627da7780f8a4fec07cb34f902464b6"
#define SHA1_5TH "2ce679528627da7780f8a4fec07cb34f902464b5"

// Support capital hex digits too
#define SHA1_6TH "2CE679528627DA7780F8A4FEC07CB34F902464B4"

int main(void)
{
    const unsigned length = SXE_HASH_SHA1_AS_CHAR_LENGTH;
    SXE_HASH *     hash;

    plan_tests(21);

    hash = sxe_hash_new("test-hash", HASH_SIZE, sizeof(SXE_HASH_KEY_VALUE_PAIR), SXE_HASH_SHA1_AS_CHAR_LENGTH, 0);
    SXEA10(hash != NULL, "Couldn't create hash 'test-hash'");

    is(sxe_hash_set   (hash, SHA1_1ST, length, 1), 4                     , "set keys 1: Inserted at index 4"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 1: Got correct value for first sha"       );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 1: Second sha is not in hash pool yet"    );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 1: Third sha is not in hash pool yet"     );

    is(sxe_hash_set   (hash, SHA1_2ND, length, 2), 3                     , "set keys 2: Inserted at index 3"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 2: Still got correct value for first sha" );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), 2                     , "set keys 2: Got correct value for second sha"      );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), SXE_HASH_KEY_NOT_FOUND, "set keys 2: Third sha is not in hash pool yet"     );

    is(sxe_hash_set   (hash, SHA1_3RD, length, 3), 2                     , "set keys 3: Inserted at index 2"                   );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), 1                     , "set keys 3: Still got correct value for first sha" );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), 2                     , "set keys 3: Still got correct value for second sha");
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), 3                     , "set keys 3: Got correct value for third sha"       );

    is(sxe_hash_set   (hash, SHA1_4TH, length, 4), 1                     , "insert too many keys: Inserted at index 1"         );
    is(sxe_hash_set   (hash, SHA1_5TH, length, 5), 0                     , "insert too many keys: Inserted at index 0"         );
    is(sxe_hash_set   (hash, SHA1_6TH, length, 6), SXE_HASH_FULL         , "insert too many keys: Failed to insert key"        );

    is(sxe_hash_delete(hash, SHA1_1ST, length   ), 1                     , "delete keys: Delete returns the correct value"     );
    is(sxe_hash_delete(hash, SHA1_2ND, length   ), 2                     , "delete keys: Delete returns the correct value"     );
    is(sxe_hash_get   (hash, SHA1_1ST, length   ), SXE_HASH_KEY_NOT_FOUND, "delete keys: First sha has been deleted"           );
    is(sxe_hash_get   (hash, SHA1_2ND, length   ), SXE_HASH_KEY_NOT_FOUND, "delete keys: Second sha has been deleted"          );
    is(sxe_hash_get   (hash, SHA1_3RD, length   ), 3                     , "delete keys: Still got correct value for third sha");

    is(sxe_hash_delete(hash, SHA1_1ST, length   ), SXE_HASH_KEY_NOT_FOUND, "delete non-existent key: returns expected value"   );

    return exit_status();
}
