/* Copyright (c) 2021 Jim Belton
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

#ifndef SXE_JITSON_H
#define SXE_JITSON_H

#include <stdbool.h>
#include <stdint.h>

#include "sxe-factory.h"

#define SXE_JITSON_TYPE_INVALID 0
#define SXE_JITSON_TYPE_NUMBER  1
#define SXE_JITSON_TYPE_STRING  2
#define SXE_JITSON_TYPE_OBJECT  3
#define SXE_JITSON_TYPE_ARRAY   4
#define SXE_JITSON_TYPE_BOOL    5
#define SXE_JITSON_TYPE_NULL    6
#define SXE_JITSON_TYPE_MEMBER  7            // Internal type for member names
#define SXE_JITSON_TYPE_MASK    0xFFFF       // Bits included in the type enumeration
#define SXE_JITSON_TYPE_IS_COPY 0            // Flag passed to API to indicate that strings/member names are to be copied
#define SXE_JITSON_TYPE_IS_REF  0x40000000    // Flag set for strings that are references (size == 0 until cached or if empty)
#define SXE_JITSON_TYPE_IS_OWN  0x80000000    // Flag set for strings that are references owned by the object (to be freed)
#define SXE_JITSON_TYPE_INDEXED 0x80000000    // Flag set for arrays and object if they have been indexed

#define SXE_JITSON_STACK_ERROR      (~0U)
#define SXE_JITSON_TOKEN_SIZE       sizeof(struct sxe_jitson)
#define SXE_JITSON_STRING_SIZE      sizeof(((struct sxe_jitson *)0)->string)

/* A jitson token. Copied strings of > 7 bytes length continue into the next token.
 */
struct sxe_jitson {
    uint32_t type;                 // See definitions above
    union {
        uint32_t size;             // Length of string or member name, number of elements/members in array/object
        uint32_t link;             // For a member in an indexed object, this is the offset of the next member in the bucket
    };
    union {
        uint32_t   *index;          // Points to offsets (size of them) to elements/members, or 0 for empty buckets
        uint64_t    integer;        // Offset of the first token past an array or object before it is indexed
        double      number;         // JSON numbers must be capable of being stored in a double wide floating point number
        bool        boolean;        // True and false
        char        string[8];      // First 8 bytes of a string, including NUL. Strings > 7 bytes long run into the next token
        const void *reference;      // Points to an external value
        struct {
            uint8_t  no_value;      // Object under construction has a member name with no value
            uint8_t  nested;        // Object or array under construction contains another array/object under construction
            uint32_t collection;    // Object or array is contained in the object or array at this index - 1 or 0 if its the root
        } partial;
    };
};

struct sxe_jitson_stack {
    unsigned           maximum;
    unsigned           count;
    struct sxe_jitson *jitsons;
    unsigned           open;        // Index + 1 of the deepest open collection that's under construction or 0 if none
};

#include "sxe-jitson-proto.h"
#include "sxe-jitson-stack-proto.h"
#include "sxe-jitson-to-json-proto.h"

#define MOCK_FAIL_STACK_NEW_OBJECT     ((char *)sxe_jitson_new + 0)
#define MOCK_FAIL_STACK_NEW_JITSONS    ((char *)sxe_jitson_new + 1)
#define MOCK_FAIL_STACK_NEXT_AFTER_GET ((char *)sxe_jitson_new + 2)
#define MOCK_FAIL_STACK_DUP            ((char *)sxe_jitson_new + 3)
#define MOCK_FAIL_STACK_NEXT           ((char *)sxe_jitson_new + 4)
#define MOCK_FAIL_OBJECT_GET_MEMBER    ((char *)sxe_jitson_new + 5)
#define MOCK_FAIL_ARRAY_GET_ELEMENT    ((char *)sxe_jitson_new + 6)

#endif
