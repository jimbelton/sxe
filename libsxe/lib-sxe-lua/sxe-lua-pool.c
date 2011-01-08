#include "sxe.h"
#include "sxe-pool.h"

#include "lua.h"
#include "ltm.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sxe-lua-pool.h"

/*
 * Each pool entry has a Lua table for use by the Lua callbacks.
 */
struct lsxe_pool_entry {
    int l_table;
};

static int
lsxe_pool_new(lua_State *L)
{
    const char *name = luaL_checkstring(L, 1);
    unsigned entries = (unsigned)luaL_checkint(L, 2);
    unsigned states = (unsigned)luaL_checkint(L, 3);
    unsigned size = (unsigned)sizeof(struct lsxe_pool_entry);
    unsigned i;
    int flags = 0;

    struct lsxe_pool_entry *pool = sxe_pool_new(name, entries, size, states, flags);

    for (i = 0; i < entries; i++) {
        lua_newtable(L);
        lua_pushinteger(L, i);
        lua_setfield(L, -2, "index");
        pool[i].l_table = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_pushlightuserdata(L, pool);
    return 1;
}

static int
lsxe_pool_get_name(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    lua_pushstring(L, sxe_pool_get_name(pool));
    return 1;
}

static int
lsxe_pool_get_number_in_state(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned state = (unsigned)luaL_checkint(L, 2);
    lua_pushinteger(L, sxe_pool_get_number_in_state(pool, state));
    return 1;
}

static int
lsxe_pool_get_oldest_element_index(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned state = (unsigned)luaL_checkint(L, 2);
    unsigned id = sxe_pool_get_oldest_element_index(pool, state);
    if (id == SXE_POOL_NO_INDEX)
        lua_pushnil(L);
    else
        lua_pushinteger(L, id);
    return 1;
}

#if TODO
static int
lsxe_pool_get_oldest_element_time(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned state = (unsigned)luaL_checkint(L, 2);
    lua_pushinteger(L, sxe_pool_get_oldest_element_time(pool, state));
    return 1;
}
#endif

static int
lsxe_pool_set_indexed_element_state(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned id = (unsigned)luaL_checkint(L, 2);
    unsigned oldst = (unsigned)luaL_checkint(L, 3);
    unsigned newst = (unsigned)luaL_checkint(L, 4);
    sxe_pool_set_indexed_element_state(pool, id, oldst, newst);
    return 0;
}

static int
lsxe_pool_set_oldest_element_state(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned oldst = (unsigned)luaL_checkint(L, 2);
    unsigned newst = (unsigned)luaL_checkint(L, 3);
    unsigned id = sxe_pool_set_oldest_element_state(pool, oldst, newst);
    if (id == SXE_POOL_NO_INDEX)
        lua_pushnil(L);
    else
        lua_pushinteger(L, id);
    return 1;
}

static int
lsxe_pool_index_to_state(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned index = (unsigned)luaL_checkint(L, 2);
    lua_pushinteger(L, sxe_pool_index_to_state(pool, index));
    return 1;
}

static int
lsxe_pool_check_timeouts(lua_State *L)
{
    (void)L;
    sxe_pool_check_timeouts();
    return 0;
}

static int
lsxe_pool_touch_indexed_element(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned index = (unsigned)luaL_checkint(L, 2);
    sxe_pool_touch_indexed_element(pool, index);
    return 0;
}

static int
lsxe_pool_get_indexed_element(lua_State *L)
{
    struct lsxe_pool_entry *pool = (luaL_checktype(L, 1, LUA_TLIGHTUSERDATA), lua_touserdata(L, 1));
    unsigned index = (unsigned)luaL_checkint(L, 2);
    lua_rawgeti(L, LUA_REGISTRYINDEX, pool[index].l_table);
    return 1;
}

static const luaL_Reg lsxe_pool_functions[] = {
    {"sxe_pool_new",                        lsxe_pool_new},
    {"sxe_pool_get_name",                   lsxe_pool_get_name},
    {"sxe_pool_get_number_in_state",        lsxe_pool_get_number_in_state},
    {"sxe_pool_get_oldest_element_index",   lsxe_pool_get_oldest_element_index},
#if TODO
    {"sxe_pool_get_oldest_element_time",    lsxe_pool_get_oldest_element_time},
#endif
    {"sxe_pool_index_to_state",             lsxe_pool_index_to_state},
    {"sxe_pool_set_indexed_element_state",  lsxe_pool_set_indexed_element_state},
    {"sxe_pool_set_oldest_element_state",   lsxe_pool_set_oldest_element_state},
    {"sxe_pool_touch_indexed_element",      lsxe_pool_touch_indexed_element},
    {"sxe_pool_check_timeouts",             lsxe_pool_check_timeouts},

    /* Would be __index metamethods if I used full userdata */
    {"sxe_pool_get_indexed_element",        lsxe_pool_get_indexed_element},

    {NULL, NULL}
};

/*****************************************************************************
 * Function registration
 ****************************************************************************/

void
register_sxe_pool(lua_State *L)
{
    int i;
    for (i = 0; lsxe_pool_functions[i].name; i++)
        lua_register(L, lsxe_pool_functions[i].name, lsxe_pool_functions[i].func);
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
