#include <stdlib.h>
#include <string.h>
#include <tap.h>

#include "sxe-factory.h"

int
main(void)
{
    struct sxe_factory *factory;
    size_t              len;
    char *              data;

    plan_tests(8);

    ok(factory = sxe_factory_alloc_new(0, 0),                        "Created a new allocating factory");
    is(sxe_factory_add(factory, "hello,", 0), 6,                     "Added 'hello,' to the factory");
    is_eq(sxe_factory_look(factory, &len), "hello,",                 "Saw 'hello,' in the factory");
    is(len, 6,                                                       "Look returned correct length");
    is(sxe_factory_add(factory, " world.xxx", 7), 7,                 "Added ' world.' to the factory");
    is_eq(sxe_factory_look(factory, NULL), "hello, world.",          "Saw 'hello, world.' in the factory");
    is_eq(data = sxe_factory_remove(factory, NULL), "hello, world.", "Removed 'hello, world.' from the factory");
    free(data);
    is(sxe_factory_look(factory, NULL), NULL,                        "Saw no data left in the factory");

    return exit_status();
}
