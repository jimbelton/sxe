/* Module that overrides lookup3 with XXH32 hash function. Calling these functions will require the libxxhash DLL.
 */

#include <string.h>
#include <xxhash.h>

#include "sxe-hash.h"

/**
 * Compute a hash sum of a fixed length or NUL terminated key using XX32 hash
 *
 * @param key    Pointer to the key
 * @param length Length of the key in bytes or 0 to use strlen
 *
 * @return 32 bit hash value
 */
unsigned
sxe_hash_xxh32(const void *key, unsigned length)
{
    if (length == 0)
        length = strlen(key);    /* COVERAGE EXCLUSION - Need a test */

    return XXH32(key, length, 17);
}

/**
 * Override the default hash sum function (lookup3) with xx32
 *
 * @param new_hash_sum Pointer to a function that takes a key and a length and returns an unsigned sum
 *
 * @note If the function is passed 0 as the length, it should use strlen to compute the length of the key
 */
void
sxe_hash_use_xxh32(void)
{
    sxe_hash_override_sum(sxe_hash_xxh32);
}
