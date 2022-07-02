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

#include <stdlib.h>
#include <string.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-factory.h"

/**
 * Contruct a new factory that uses allocated memory
 *
 * @param minsize Minimum bytes of space allocated to the factory
 * @param maxincr Once the data space reaches this size, it will be increased by this amount
 */
void
sxe_factory_alloc_make(struct sxe_factory *factory, size_t minsize, size_t maxincr)
{
    factory->len     = 0;
    factory->size    = minsize ?: 8;
    factory->maxincr = maxincr ?: 4096;
    factory->data    = NULL;
}

/**
 * Reserve space in a factory. Reserved space always immediately follows the space already used.
 *
 * @param factory The factory
 * @param len     The amount of space to reserve
 *
 * @return A pointer to the reserved space or NULL on allocation failure
 */
char *
sxe_factory_reserve(struct sxe_factory *factory, size_t len)
{
    char  *newdata;
    size_t needed = factory->len + len + 1;

    if (needed > factory->size || factory->data == NULL) {
        size_t newsize = factory->size;

        while (newsize < needed)
            newsize = newsize >= factory->maxincr ? newsize + factory->maxincr : newsize << 1;

        if ((newdata = MOCKFAIL(sxe_factory_reserve, NULL, sxe_realloc(factory->data, newsize))) == NULL)
            return NULL;

        factory->data = newdata;
        factory->size = newsize;
    }

    return &factory->data[factory->len];
}

/**
 * Commit data to a factory. Data must be stored in reserved space prior to commiting.
 *
 * @param factory The factory
 * @param len     The amount of data to commit
 */
void
sxe_factory_commit(struct sxe_factory *factory, size_t len)
{
    factory->len += len;
    factory->data[factory->len] = '\0';
}

/**
 * Add data to a factory.
 *
 * @param factory The factory
 * @param data    Data to be added
 * @param len     The amount of data to add
 */
ssize_t
sxe_factory_add(struct sxe_factory *factory, const char *data, size_t len)
{
    char *reservation;

    if (!(reservation = sxe_factory_reserve(factory, len = len ?: strlen(data))))
        return -1;

    memcpy(reservation, data, len);
    sxe_factory_commit(factory, len);
    return len;
}

/**
 * Look at the current data in a factory
 *
 * @param factory The factory
 * @param len_out Pointer to a variable in which to save the length or NULL.
 *
 * @return The current data or NULL if no data has been allocated
 */
void *
sxe_factory_look(struct sxe_factory *factory, size_t *len_out)
{
    if (len_out)
        *len_out = factory->len;

    return factory->data;
}

/**
 * Remove the current data from a factory
 *
 * @param factory The factory
 * @param len_out Pointer to a variable in which to save the length or NULL.
 *
 * @return The data removed from the factory or NULL if no data has been allocated or realloc failed
 */
void *
sxe_factory_remove(struct sxe_factory *factory, size_t *len_out)
{
    char * data = sxe_factory_look(factory, len_out);

    if (factory->len + 1 < factory->size)
        data = sxe_realloc(factory->data, factory->len + 1);

    factory->data = NULL;
    factory->len  = 0;
    return data;
}
