#include <string.h>

#include "tap.h"
#include "sxe.h"
#include "sxe-lua.h"

/*****************************************************************************
 * Main!
 ****************************************************************************/

int
main(void)
{
    SXE_LUA *sxel = sxe_lua_new();

    SXEL10("sxelua: loading sxelua handlers");
    sxe_lua_load(sxel, "../test/test-sxe-lua.lua");

    sxe_init();
    sxe_lua_init(sxel);
    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);

    return exit_status();
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
