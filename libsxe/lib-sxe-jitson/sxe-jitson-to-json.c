#include <stdio.h>

#include "sxe-factory.h"
#include "sxe-log.h"
#include "sxe-jitson.h"

#define DOUBLE_MAX_LEN 24

char *
sxe_jitson_build_json(struct sxe_jitson *jitson, struct sxe_factory *factory)
{
    char    *reservation;
    unsigned i, len;
    uint32_t index, type;

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
            sxe_factory_add(factory, jitson->member.name, len);
        else
            sxe_factory_add(factory, sxe_jitson_get_string(jitson, NULL), len);

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
    sxe_jitson_build_json(jitson, factory);
    return sxe_factory_remove(factory, len_out);
}
