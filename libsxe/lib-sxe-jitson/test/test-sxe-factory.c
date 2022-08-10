#include <stdlib.h>
#include <string.h>
#include <tap.h>

#include "mockfail.h"
#include "sxe-alloc.h"
#include "sxe-factory.h"

int
main(void)
{
    struct sxe_factory factory[1];
    size_t             len;
    uint64_t           start_allocations;
    char *             data;

    plan_tests(9);

    start_allocations     = sxe_allocations;
    sxe_alloc_diagnostics = true;
    sxe_factory_alloc_make(factory, 0, 0);

    MOCKFAIL_START_TESTS(1, sxe_factory_reserve);
    is(sxe_factory_add(factory, "hello,", 0), -1,                    "Failed to add 'hello,' to the factory on realloc failure");
    MOCKFAIL_END_TESTS();

    is(sxe_factory_add(factory, "hello,", 0), 6,                     "Added 'hello,' to the factory");
    is_eq(sxe_factory_look(factory, &len), "hello,",                 "Saw 'hello,' in the factory");
    is(len, 6,                                                       "Look returned correct length");
    is(sxe_factory_add(factory, " world.xxx", 7), 7,                 "Added ' world.' to the factory");
    is_eq(sxe_factory_look(factory, NULL), "hello, world.",          "Saw 'hello, world.' in the factory");
    is_eq(data = sxe_factory_remove(factory, NULL), "hello, world.", "Removed 'hello, world.' from the factory");
    sxe_free(data);
    is(sxe_factory_look(factory, NULL), NULL,                        "Saw no data left in the factory");

    is(sxe_allocations, start_allocations,                           "No memory was leaked");
    return exit_status();
}
