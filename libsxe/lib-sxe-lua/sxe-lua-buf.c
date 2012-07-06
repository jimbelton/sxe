#include <string.h>

#include "sxe.h"
#include "sxe-pool.h"

#include "lua.h"
#include "ltm.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sxe-lua-buf.h"

#define LSXE_BUF "SXEBUF"

struct lsxe_buf {
    unsigned   *len;    /* pointer to length of buffer */
    const char *ptr;    /* pointer to buffer */
};

const char *
lsxe_checkbuf(lua_State *L, int n, size_t *len)
{
    struct lsxe_buf *buf = luaL_checkudata(L, n, LSXE_BUF);
    if (len)
        *len = *buf->len;
    return buf->ptr;
}

int
lsxe_buf_new_from_sxe(lua_State *L, SXE *sxe)
{
    struct lsxe_buf *buf = lua_newuserdata(L, sizeof *buf);

    buf->len = &sxe->in_total;
    buf->ptr =  sxe->in_buf;

    luaL_getmetatable(L, LSXE_BUF);
    lua_setmetatable(L, -2);

    return 1;
}

static int
findbuf(lua_State *L, int icase)
{
    struct lsxe_buf *buf = luaL_checkudata(L, 1, LSXE_BUF);
    const char *str = luaL_checkstring(L, 2);
    int offset = 0;
    const char *found;
    const char *start;
    unsigned length;

    SXEL6("findbuf[len=%d]: searching for %s", *buf->len, str);
    if (lua_gettop(L) == 3) {
        int startpos = luaL_checkint(L, 3);
        SXEL6("findbuf: startpos=%d", startpos);
        if (startpos > 0)
            offset = startpos - 1;
        else if (startpos < 0)
            offset = *buf->len + startpos;
        SXEL6("findbuf: offset=%d", offset);
        if (offset < 0 || (unsigned)offset >= *buf->len) {
            lua_pushnil(L);
            return 1;
        }
    }

    SXEL6("findbuf: searching for string %s at offset %d\n", str, offset);
    start  =  buf->ptr + offset;
    length = *buf->len - offset;

    if (icase)
        found = sxe_strncasestr(start, str, length);
    else
        found = sxe_strnstr(start, str, length);

    if (found) {
        SXEL6("findbuf: found match at offset %u (pos %u)", SXE_CAST(unsigned, found - start), SXE_CAST(unsigned, 1 + found - start));
        lua_pushinteger(L, 1 + found - start); /* Lua counts from 1 */
    }
    else {
        SXEL6("findbuf: did not match");
        lua_pushnil(L);
    }
    return 1;
}

static int
lsxe_buf_find(lua_State *L)
{
    return findbuf(L, 0);
}

static int
lsxe_buf_ifind(lua_State *L)
{
    return findbuf(L, 1);
}

static const luaL_Reg lsxe_buf_methods[] = {
    {"find", lsxe_buf_find},
    {"ifind", lsxe_buf_ifind},
    {NULL, NULL}
};

void
register_sxe_buf(lua_State *L)
{
    luaL_newmetatable(L, LSXE_BUF);
    lua_pushvalue(L, -1);
    lua_setfield(L, -1, "__index");
    luaL_register(L, NULL, lsxe_buf_methods);
    lua_pop(L, 1);
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
