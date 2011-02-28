#include <string.h>

#include "sxe.h"
#include "sxe-util.h"

#include "lua.h"
#include "ltm.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sxe-lua-utils.h"
#include "sxe-lua-timer.h"

struct lsxe_timer {
    ev_timer tm;
    lua_State *L;
    int l_timer;
};

static void
lsxe_timer_event(EV_P_ ev_timer *timer, int revents) /* Coverage Exclusion - todo: win32 coverage */
{
    struct lsxe_timer *tm = timer->data; /* Coverage Exclusion - todo: win32 coverage */

#ifdef EV_MULTIPLICITY
    SXE_UNUSED_PARAMETER(loop);
#endif
    SXE_UNUSED_PARAMETER(revents);

    lua_rawgeti(tm->L, LUA_REGISTRYINDEX, tm->l_timer); /* Coverage Exclusion - todo: win32 coverage */
    lua_call(tm->L, 0, 0);
}

static int
lsxe_timer_new(lua_State *L) /* Coverage Exclusion - todo: win32 coverage */
{
    double delay = luaL_checknumber(L, 2); /* Coverage Exclusion - todo: win32 coverage */
    double freq = luaL_checknumber(L, 3);
    struct lsxe_timer *t;
    int ref;

    lsxe_checkfunction(L, 1); /* Coverage Exclusion - todo: win32 coverage */
    lua_pushvalue(L, 1);
    ref = luaL_ref(L, LUA_REGISTRYINDEX);

    t = lua_newuserdata(L, sizeof(struct lsxe_timer)); /* Coverage Exclusion - todo: win32 coverage */
    sxe_timer_init(&t->tm, lsxe_timer_event, delay, freq);
    t->tm.data = t;
    t->L = L;
    t->l_timer = ref;

    return 1; /* Coverage Exclusion - todo: win32 coverage */
}

static int
lsxe_timer_start(lua_State *L) /* Coverage Exclusion - todo: win32 coverage */
{
    struct lsxe_timer *t = (luaL_checktype(L, 1, LUA_TUSERDATA), lua_touserdata(L, 1)); /* Coverage Exclusion - todo: win32 coverage */
    sxe_timer_start(&t->tm);
    return 0;
}

static int
lsxe_timer_stop(lua_State *L) /* Coverage Exclusion - todo: win32 coverage */
{
    struct lsxe_timer *t = (luaL_checktype(L, 1, LUA_TUSERDATA), lua_touserdata(L, 1)); /* Coverage Exclusion - todo: win32 coverage */
    sxe_timer_stop(&t->tm);
    return 0;
}

static const luaL_Reg lsxe_timer_functions[] = {
    {"sxe_timer_new",   lsxe_timer_new},
    {"sxe_timer_start", lsxe_timer_start},
    {"sxe_timer_stop", lsxe_timer_stop},
    {NULL, NULL}
};

/*****************************************************************************
 * Function registration
 ****************************************************************************/

void
register_sxe_timer(lua_State *L)
{
    int i;
    for (i = 0; lsxe_timer_functions[i].name; i++)
        lua_register(L, lsxe_timer_functions[i].name, lsxe_timer_functions[i].func);
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
