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

#include <stdio.h>
#include <string.h>

#include "sxe-factory.h"
#include "sxe-log.h"
#include "sxe-jitson.h"

#define DOUBLE_MAX_LEN 24

char *
sxe_jitson_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char       *reservation;
    const char *string;
    unsigned    first, i, len;
    uint32_t    index, type;
    char        unicode_str[8];

    switch (type = sxe_jitson_get_type(jitson)) {
    case SXE_JITSON_TYPE_NUMBER:
        if ((reservation = sxe_factory_reserve(factory, DOUBLE_MAX_LEN)) == NULL)
            return NULL;

        len = snprintf(reservation, DOUBLE_MAX_LEN + 1, "%G", sxe_jitson_get_number(jitson));
        SXEA6(len <= DOUBLE_MAX_LEN, "As a string, numeric value %G is more than %u characters long",
              sxe_jitson_get_number(jitson), DOUBLE_MAX_LEN);
        sxe_factory_commit(factory, len);
        return reservation;

    case SXE_JITSON_TYPE_MEMBER:
    case SXE_JITSON_TYPE_STRING:
        len = sxe_jitson_get_size(jitson);
        sxe_factory_add(factory, "\"", 1);

        if (type == SXE_JITSON_TYPE_MEMBER)
            string = jitson->member.name;
        else
            string = sxe_jitson_get_string(jitson, NULL);

        for (first = i = 0; i < len; i++)
            /* If the character is a control character or " or \, encode it as a unicode escape sequence.
             * (unsigned char) casts are used to allow any UTF8 encoded string.
             */
            if ((unsigned char)string[i] <= 0x1F || string[i] == '"' || string[i] == '\\') {
                if (first < i)
                    sxe_factory_add(factory, &string[first], i - first);

                snprintf(unicode_str, sizeof(unicode_str), "\\u00%02x", (unsigned char)string[i]);
                SXEA6(strlen(unicode_str) == 6, "Unicode escape sequence should always be 6 characters long");
                sxe_factory_add(factory, unicode_str, 6);
                first = i + 1;
            }

        if (first < len)
            sxe_factory_add(factory, &string[first], len - first);

        sxe_factory_add(factory, "\"", 1);
        break;

    case SXE_JITSON_TYPE_BOOL:
        if (sxe_jitson_get_bool(jitson))
            sxe_factory_add(factory, "true", 4);
        else
            sxe_factory_add(factory, "false", 5);

        break;

    case SXE_JITSON_TYPE_NULL:
        sxe_factory_add(factory, "null", 4);
        break;

    case SXE_JITSON_TYPE_ARRAY:
        len = sxe_jitson_get_size(jitson);
        sxe_factory_add(factory, "[", 1);

        for (i = 0; i < len - 1; i++) {
            sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, i), factory);
            sxe_factory_add(factory, ",", 1);
        }

        if (len)
            sxe_jitson_build_json(sxe_jitson_array_get_element(jitson, len - 1), factory);

        sxe_factory_add(factory, "]", 1);
        break;

    case SXE_JITSON_TYPE_OBJECT:
        if ((len = sxe_jitson_get_size(jitson)) == 0) {
            sxe_factory_add(factory, "{}", 2);
            break;
        }

        if (!(jitson->type & SXE_JITSON_TYPE_INDEXED))    // Force indexing. Would it be better to walk the unindexed oject?
            sxe_jitson_object_get_member(jitson, "", 0);

        *sxe_factory_reserve(factory, 1) = '{';

        for (i = 0; i < len; i++) {                                            // For each bucket
            for (index = jitson->index[i]; index; index = jitson[index].link) {    // For each member in the bucket
                sxe_factory_commit(factory, 1);                                    // Commit the '{' or ','
                sxe_jitson_build_json(&jitson[index], factory);                    // Output the member name
                sxe_factory_add(factory, ":", 1);
                sxe_jitson_build_json(SXE_JITSON_MEMBER_SKIP(&jitson[index]), factory);    // Output the value
                *sxe_factory_reserve(factory, 1) = ',';
            }
        }

        *sxe_factory_reserve(factory, 1) = '}';    // Repeating a reservation is safe, allowing the last ',' to be overwritten
        sxe_factory_commit(factory, 1);
        break;
    }

    return sxe_factory_look(factory, NULL);
}

char *
sxe_jitson_to_json(struct sxe_jitson *jitson, size_t *len_out)
{
    struct sxe_factory factory[1];

    sxe_factory_alloc_make(factory, 0, 0);
    return sxe_jitson_build_json(jitson, factory) ? sxe_factory_remove(factory, len_out) : NULL;
}
