#include <string.h>

#include "tap.h"
#include "sxe.h"
#include "sxe-lua.h"

/*****************************************************************************
 * Main!
 ****************************************************************************/

static int           s_bool   =  0;
static int           s_int    = 99;
static unsigned      s_uint   = 88;
static long          s_long   = 77;
static unsigned long s_ulong  = 66;
static char          s_string[1024];

int
main(void)
{
    SXE_LUA *sxel = sxe_lua_new();

    snprintf(s_string, sizeof s_string, "Goodbye, cruel world");

    sxe_lua_tie_field(sxel, "test", "bool",  'b', &s_bool);
    sxe_lua_tie_field(sxel, "test", "int",   'i', &s_int);
    sxe_lua_tie_field(sxel, "test", "uint",  'i', &s_uint);
    sxe_lua_tie_field(sxel, "test", "long",  'l', &s_long);
    sxe_lua_tie_field(sxel, "test", "ulong", 'l', &s_ulong);
    sxe_lua_tie_field(sxel, "test", "str",   's', s_string, sizeof s_string);

    SXEL10("sxelua: loading sxelua handlers");
    sxe_lua_load(sxel, "../test/test-sxe-lua.lua");

    sxe_init();
    sxe_lua_init(sxel);
    ev_loop(ev_default_loop(EVFLAG_AUTO), 0);

    is_eq(s_string, "Hello, world", "tied string set to 'Hello, world'");
    is(s_int, 10, "tied integer set to '10'");
    is(s_uint, 128, "tied unsigned set to '128'");
    is(s_long, 42L, "tied long set to '42'");
    is(s_ulong, -2UL, "tied unsigned long set to '-2'");
    is(s_bool, 1, "tied boolean set to '1'");

    return exit_status();
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
