#include <string.h>

#include "lookup3.h"
#include "sxe-hash.h"

/* Default sum function that uses lookup3
 */
static unsigned
sxe_hash_def(const void *key, unsigned length)
{
    if (length == 0)
        length = strlen(key);

#if HASH_LITTLE_ENDIAN
    return hashlittle(key, length, 0);
#else
    return hashbig(key, length, 0);
#endif
}

/**
 * Compute a hash sum of a fixed length or NUL terminated key
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 32 bit hash value
 */
unsigned (*sxe_hash_sum)(const void *key, unsigned length) = sxe_hash_def;

/**
 * Override the default hash sum function (lookup3)
 *
 * @param new_hash_sum Pointer to a function that takes a key and a length and returns an unsigned sum
 *
 * @note If the function is passed 0 as the length, it should use strlen to compute the length of the key
 */
SXE_HASH_FUNC
sxe_hash_override_sum(unsigned (*new_hash_sum)(const void *key, unsigned length))
{
    SXE_HASH_FUNC old_hash_sum = sxe_hash_sum;

    sxe_hash_sum = new_hash_sum;
    return old_hash_sum;
}
