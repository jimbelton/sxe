#include <string.h>
#include <stdarg.h>

#include "sxe.h"
#include "sxe-spawn.h"

#include "lua.h"
#include "ltm.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sxe-lua-utils.h"
#include "sxe-lua.h"
#include "sxe-lua-buf.h"
#include "sxe-lua-pool.h"
#include "sxe-lua-tap.h"
#include "sxe-lua-timer.h"

static int lsxe_new_tcp      (lua_State *);
static int lsxe_buf          (lua_State *);
static int lsxe_buf_used     (lua_State *);
static int lsxe_peer_addr    (lua_State *);
static int lsxe_peer_port    (lua_State *);
static int lsxe_local_addr   (lua_State *);
static int lsxe_local_port   (lua_State *);
static int lsxe_id           (lua_State *);
static int lsxe_buf_clear    (lua_State *);
static int lsxe_write        (lua_State *);
static int lsxe_close        (lua_State *);
static int lsxe_listen       (lua_State *);
static int lsxe_connect      (lua_State *);
static int lsxe_meta_index   (lua_State *);
static int lsxe_meta_newindex(lua_State *);

#ifndef WINDOWS_NT
static int lsxe_spawn        (lua_State *);
static int lsxe_spawn_kill   (lua_State *);
#endif

static const luaL_Reg lsxe_sxe_functions[] = {
    /* Globals */
    {"sxe_new_tcp",     lsxe_new_tcp},
#ifndef WINDOWS_NT
    {"sxe_spawn",       lsxe_spawn},
    {"sxe_spawn_kill",  lsxe_spawn_kill},
#endif

    /* These *could* be implemented as methods on SXE objects */
    {"sxe_buf",         lsxe_buf},
    {"sxe_buf_used",    lsxe_buf_used},
    {"sxe_peer_addr",   lsxe_peer_addr},
    {"sxe_peer_port",   lsxe_peer_port},
    {"sxe_local_addr",  lsxe_local_addr},
    {"sxe_local_port",  lsxe_local_port},
    {"sxe_id",          lsxe_id},

    /* These are also exposed as methods */
    {"sxe_buf_clear",   lsxe_buf_clear},
    {"sxe_write",       lsxe_write},
    {"sxe_close",       lsxe_close},
    {"sxe_listen",      lsxe_listen},
    {"sxe_connect",     lsxe_connect},

    {NULL, NULL}
};

static const luaL_Reg lsxe_sxe_properties[] = {
    {"id",          lsxe_id},
    {"buf",         lsxe_buf},
    {"buf_used",    lsxe_buf_used},
    {"peer_addr",   lsxe_peer_addr},
    {"peer_port",   lsxe_peer_port},
    {"local_addr",  lsxe_local_addr},
    {"local_port",  lsxe_local_port},
    {NULL, NULL}
};

static const luaL_Reg lsxe_sxe_methods[] = {
    {"buf_clear",       lsxe_buf_clear},
    {"write",           lsxe_write},
    {"close",           lsxe_close},
    {"listen",          lsxe_listen},
    {"connect",         lsxe_connect},
#ifndef WINDOWS_NT
    {"kill",            lsxe_spawn_kill},
#endif
    {"__index",         lsxe_meta_index},
    {"__newindex",      lsxe_meta_newindex},
    {NULL, NULL}
};

/*****************************************************************************
 * sxe_register(connections, initializer_function)
 * - calls sxe_register(connections)
 * - queues the initializer_function for calling after sxe_init()
 ****************************************************************************/

static int
lsxe_register(lua_State *L)
{
    int count = luaL_checkint(L, 1);

    lsxe_checkfunction(L, 2);
    sxe_register(count, 0);

    lua_pushinteger(L, 1 + lua_objlen(L, lua_upvalueindex(1))); /* index */
    lua_pushvalue(L, 2); /* function */
    lua_settable(L, lua_upvalueindex(1)); /* table[index] = function */

    return 0;
}

/*****************************************************************************
 * initialize_plugins()
 * - calls each Lua module's init function (passed to sxe_register())
 ****************************************************************************/

static void
initialize_plugins(lua_State *L)
{
    int t = lua_gettop(L);
    lua_pushnil(L);
    while (lua_next(L, t) != 0) {
        lua_pushvalue(L, -1);
        lua_call(L, 0, 0);
        lua_pop(L, 1);
    }
}

/*****************************************************************************
 * SXEL1(), SXEL2(), etc.
 * - logs to the SXE log
 ****************************************************************************/
#define SXELn(n) static int lsxe_SXEL ## n(lua_State *L){\
    const char *msg = luaL_checkstring(L, 1); (void)msg; \
    SXEL ## n ## 1("%s", msg); \
    return 0; }
SXELn(1);
SXELn(2);
SXELn(3);
SXELn(4);
SXELn(5);
SXELn(6);
SXELn(7);
SXELn(8);
SXELn(9);

/*****************************************************************************
 * We expose SXE* to Lua as a 'userdata' object, which is an allocated chunk
 * of memory with a virtual function table. We allocate only enough memory to
 * store a pointer to the actual SXE object, plus references to the Lua
 * callbacks.
 ****************************************************************************/
#define LTYPE_SXE "SXE"

struct lsxe_udata {
    SXE *sxe;       /* underlying SXE object being wrapped */
    lua_State *L;   /* the Lua interpreter */
    int l_obj;      /* reference to the Lua userdata object represented by this connection */
    int f_evconn;   /* reference to the Lua 'connect' callback function */
    int f_evread;   /* reference to the Lua 'read' callback function */
    int f_evclose;  /* reference to the Lua 'close' callback function */
    pid_t pid;      /* pid (if spawned; otherwise 0) */
};

static struct lsxe_udata *
lsxe_newsxe(lua_State *L, SXE *sxe)
{
    struct lsxe_udata *udata = lua_newuserdata(L, sizeof *udata);

    udata->L = L;
    udata->sxe = sxe;
    udata->f_evconn = udata->f_evread = udata->f_evclose = LUA_REFNIL;
    udata->pid = 0;
    SXE_USER_DATA(sxe) = udata;

    lua_newtable(L);
    luaL_register(L, NULL, lsxe_sxe_methods);
    lua_pushstring(L, LTYPE_SXE);
    lua_setfield(L, -2, "__name__");
    lua_setmetatable(L, -2);

    udata->l_obj = luaL_ref(L, LUA_REGISTRYINDEX);
    return udata;
}

static void *
lsxe_checksxe(lua_State *L, int idx)
{
    const char *msg;
    void *p = lua_touserdata(L, idx);
    if (p != NULL) {
        if (lua_getmetatable(L, idx)) {
            lua_getfield(L, -1, "__name__");
            lua_pushstring(L, LTYPE_SXE);
            if (lua_rawequal(L, -1, -2)) {
                lua_pop(L, 2);
                return p;
            }
        }
    }
    msg = lua_pushfstring(L, "SXE object expected, got %s", luaL_typename(L, idx));
    luaL_argerror(L, idx, msg);
    return NULL; /* COVERAGE EXCLUSION: not reached */
}

/*****************************************************************************
 * SXE global functions that operate on SXE objects passed to Lua callbacks.
 ****************************************************************************/

static void
handle_sxe_connect(SXE * this)
{
    struct lsxe_udata *udata = SXE_USER_DATA(this);
    if (udata->f_evconn != LUA_REFNIL) {
        SXEL61("handle_sxe_connect: calling lua function ref %d", udata->f_evconn);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->f_evconn);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->l_obj);
        lua_call(udata->L, 1, 0);
    }
}

static void
handle_sxe_read(SXE * this, int length)
{
    struct lsxe_udata *udata = SXE_USER_DATA(this);
    if (udata->f_evread != LUA_REFNIL) {
        SXEL61("handle_sxe_read: calling lua function ref %d", udata->f_evread);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->f_evread);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->l_obj);
        lua_pushinteger(udata->L, length);
        lua_call(udata->L, 2, 0);
    }
}

static int
lsxe_ref_again(lua_State *L, int oldref)
{
    lua_rawgeti(L, LUA_REGISTRYINDEX, oldref);
    return luaL_ref(L, LUA_REGISTRYINDEX);
}

static void
lsxe_unreference(struct lsxe_udata *udata)
{
    luaL_unref(udata->L, LUA_REGISTRYINDEX, udata->l_obj);
    udata->l_obj = LUA_REFNIL;
    if (udata->f_evconn != LUA_REFNIL)
        luaL_unref(udata->L, LUA_REGISTRYINDEX, udata->f_evconn);
    udata->f_evconn = LUA_REFNIL;
    if (udata->f_evread != LUA_REFNIL)
        luaL_unref(udata->L, LUA_REGISTRYINDEX, udata->f_evread);
    udata->f_evread = LUA_REFNIL;
    if (udata->f_evclose != LUA_REFNIL)
        luaL_unref(udata->L, LUA_REGISTRYINDEX, udata->f_evclose);
    udata->f_evclose = LUA_REFNIL;
}

static void
handle_sxe_close(SXE * this)
{
    struct lsxe_udata *udata = SXE_USER_DATA(this);

    if (udata->f_evclose != LUA_REFNIL) {
        SXEL61("handle_sxe_close: calling lua function ref %d", udata->f_evclose);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->f_evclose);
        lua_rawgeti(udata->L, LUA_REGISTRYINDEX, udata->l_obj);
        lua_call(udata->L, 1, 0);
    }

    lsxe_unreference(udata);
}

static void
handle_tcp_connect(SXE * this)
{
    struct lsxe_udata *odata, *udata;

    /* Create a new Lua object to represent the new tcp connection. Take new
     * references to the same functions. */
    odata = (struct lsxe_udata *)SXE_USER_DATA(this);
    udata = lsxe_newsxe(odata->L, this);
    udata->f_evconn = lsxe_ref_again(odata->L, odata->f_evconn);
    udata->f_evread = lsxe_ref_again(odata->L, odata->f_evread);
    udata->f_evclose = lsxe_ref_again(odata->L, odata->f_evclose);
    SXE_USER_DATA(this) = udata;

    handle_sxe_connect(this);
}

static int
lsxe_new_tcp(lua_State *L)
{
    struct lsxe_udata *udata;
    const char *host;
    int port;
    SXE *sxe;
    int n = lua_gettop(L);

    SXEL10("lsxe_new_tcp()");

    if (n < 2 || n > 5)
        THROW("sxe_new_tcp: expected 2-5 args, called with %d", n);
    host = luaL_checkstring(L, 1);
    port = luaL_checkint(L, 2);
    if (n >= 3 && lua_type(L, 3) != LUA_TFUNCTION && lua_type(L, 3) != LUA_TNIL)
        THROW("sxe_new_tcp: arg 3: invalid type %s", luaL_typename(L, 3));
    if (n >= 4 && lua_type(L, 4) != LUA_TFUNCTION && lua_type(L, 4) != LUA_TNIL)
        THROW("sxe_new_tcp: arg 4: invalid type %s", luaL_typename(L, 4));
    if (n >= 5 && lua_type(L, 5) != LUA_TFUNCTION && lua_type(L, 5) != LUA_TNIL)
        THROW("sxe_new_tcp: arg 5: invalid type %s", luaL_typename(L, 5));

    sxe = sxe_new_tcp(NULL, host, port, handle_tcp_connect, handle_sxe_read, handle_sxe_close);

    udata = lsxe_newsxe(L, sxe);

    if (n >= 3) {
        lua_pushvalue(L, 3);
        udata->f_evconn = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (n >= 4) {
        lua_pushvalue(L, 4);
        udata->f_evread = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (n >= 5) {
        lua_pushvalue(L, 5);
        udata->f_evclose = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, udata->l_obj);
    return 1;
}

#ifndef WINDOWS_NT
/*****************************************************************************
 * Spawn methods. We use the same callbacks as TCP.
 ****************************************************************************/

static int
lsxe_spawn(lua_State *L)
{
    SXE_SPAWN s;
    int n = lua_gettop(L);
    struct lsxe_udata *udata;
    const char *command = luaL_checkstring(L, 1);
    const char *arg1    = NULL;
    const char *arg2    = NULL;

    if (n >= 2 && lua_type(L, 2) != LUA_TNIL)
        arg1 = luaL_checkstring(L, 2);
    if (n >= 3 && lua_type(L, 3) != LUA_TNIL)
        arg2 = luaL_checkstring(L, 3);
    if (n >= 4 && lua_type(L, 4) != LUA_TFUNCTION && lua_type(L, 4) != LUA_TNIL)
        THROW("sxe_spawn: arg 4: invalid type %s", luaL_typename(L, 4));
    if (n >= 5 && lua_type(L, 5) != LUA_TFUNCTION && lua_type(L, 5) != LUA_TNIL)
        THROW("sxe_spawn: arg 5: invalid type %s", luaL_typename(L, 5));
    if (n >= 6 && lua_type(L, 6) != LUA_TFUNCTION && lua_type(L, 6) != LUA_TNIL)
        THROW("sxe_spawn: arg 6: invalid type %s", luaL_typename(L, 6));

    sxe_spawn(NULL, &s, command, arg1, arg2, handle_sxe_connect, handle_sxe_read, handle_sxe_close);

    udata = lsxe_newsxe(L, s.sxe);
    udata->pid = s.pid;

    if (n >= 4) {
        lua_pushvalue(L, 4);
        udata->f_evconn = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (n >= 5) {
        lua_pushvalue(L, 5);
        udata->f_evread = luaL_ref(L, LUA_REGISTRYINDEX);
    }
    if (n >= 6) {
        lua_pushvalue(L, 6);
        udata->f_evclose = luaL_ref(L, LUA_REGISTRYINDEX);
    }

    lua_rawgeti(L, LUA_REGISTRYINDEX, udata->l_obj);
    return 1;
}

static int
lsxe_spawn_kill(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    int sig = luaL_checkint(L, 2);
    SXE_SPAWN s;

    if (udata->pid == 0)
        THROW("sxe_spawn_kill() called on non-spawned SXE object");

    s.sxe = udata->sxe;
    s.pid = udata->pid;
    sxe_spawn_kill(&s, sig);

    /* momentarily, the onclose event will fire, and destroy the object */
    return 0;
}
#endif

/*****************************************************************************
 * SXE methods, exposed through the SXE metatable. These are also exposed as
 * global functions, too. Lua methods are simply functions whose first
 * argument is 'self', so they can be exposed as global functions or methods.
 ****************************************************************************/

static int
lsxe_listen(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    sxe_listen(udata->sxe);
    return 0;
}

static int
lsxe_connect(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    const char *addr = luaL_checkstring(L, 2);
    int port = luaL_checkint(L, 3);
    sxe_connect(udata->sxe, addr, port);
    return 0;
}

static int
lsxe_write(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    size_t length;
    const char *data = NULL;

    switch (lua_type(L, 2)) {
        case LUA_TNUMBER:
        case LUA_TSTRING:
            data = luaL_checklstring(L, 2, &length);
            break;
        case LUA_TUSERDATA:
            data = lsxe_checkbuf(L, 2, &length);
            break;
        default:
            THROW("sxe_write: arg 2: invalid type %s", luaL_typename(L, 2));
    }

    SXEL63("Writing %d:%.*s to socket", (int)length, (int)length, data);
    sxe_write(udata->sxe, data, length);
    return 0;
}

static int
lsxe_close(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    sxe_close(udata->sxe);
    lsxe_unreference(udata);
    return 0;
}

static int
lsxe_buf(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    return lsxe_buf_new_from_sxe(L, udata->sxe);
}

static int
lsxe_buf_used(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    lua_pushinteger(L, SXE_BUF_USED(udata->sxe));
    return 1;
}

static int
lsxe_buf_clear(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    sxe_buf_clear(udata->sxe);
    return 0;
}

static void
push_address(lua_State *L, SXE *sxe, int remote)
{
    char ip_addr[50];
    int addr = ntohl(
            (remote ? SXE_PEER_ADDR(sxe) : SXE_LOCAL_ADDR(sxe))
            ->sin_addr.s_addr);
    snprintf(ip_addr, sizeof ip_addr, "%d.%d.%d.%d", addr >> 24, (addr >> 16) & 0xff, (addr >> 8) & 0xff, addr & 0xff);
    lua_pushstring(L, ip_addr);
}

static int
lsxe_peer_addr(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    push_address(L, udata->sxe, 1);
    return 1;
}

static int
lsxe_local_addr(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    push_address(L, udata->sxe, 0);
    return 1;
}

static int
lsxe_peer_port(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    lua_pushinteger(L, SXE_PEER_PORT(udata->sxe));
    return 1;
}

static int
lsxe_local_port(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    lua_pushinteger(L, SXE_LOCAL_PORT(udata->sxe));
    return 1;
}

static int
lsxe_id(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    lua_pushinteger(L, SXE_ID(udata->sxe));
    return 1;
}

static int
lsxe_meta_index(lua_State *L)
{
    const char *key = luaL_checkstring(L, 2);
    int i;

    lsxe_checksxe(L, 1);
    lua_getmetatable(L, 1);
    lua_pushvalue(L, 2);
    lua_rawget(L, -2);
    lua_remove(L, -2); /* remove the metatable */
    if (lua_type(L, -1) != LUA_TNIL)
        return 1; /* return the metatable entry */
    lua_pop(L, 1); /* pop nil */

    /* Check for properties: methods that we auto-call */
    for (i = 0; lsxe_sxe_properties[i].name; i++) {
        if (0 == strcmp(key, lsxe_sxe_properties[i].name))
            return lsxe_sxe_properties[i].func(L);
    }

    lua_pushnil(L);
    return 1;
}

static int
lsxe_meta_newindex(lua_State *L)
{
    struct lsxe_udata *udata = (struct lsxe_udata *)lsxe_checksxe(L, 1);
    const char *key = luaL_checkstring(L, 2);
    int *ref;

    SXEL12("lsxe_meta_newindex: this[%s] = {%s}\n", key, luaL_typename(L, 3));

    if (0 == strcmp(key, "onread"))
        ref = &udata->f_evread;
    else if (0 == strcmp(key, "onconnect"))
        ref = &udata->f_evconn;
    else if (0 == strcmp(key, "onclose"))
        ref = &udata->f_evclose;
    else {
        lua_getmetatable(L, 1); /* get SXE metatable */
        lua_pushvalue(L, 2);    /* push key */
        lua_pushvalue(L, 3);    /* push value */
        lua_rawset(L, -3);      /* set it */
        lua_pop(L, 1);
        return 0;
    }

    SXEL13("previously, was %s nil? %d (%d)", key, *ref == LUA_REFNIL, *ref);
    if (*ref != LUA_REFNIL)
        luaL_unref(L, LUA_REGISTRYINDEX, *ref);
    lua_pushvalue(L, 3);
    *ref = luaL_ref(L, LUA_REGISTRYINDEX);
    SXEL13("now, is %s nil? %d (%d)", key, *ref == LUA_REFNIL, *ref);

    return 0;
}

#if SXE_DEBUG
static void
fdump(FILE *f, lua_State *L, int n)
{
    int t;

    if (n < 0)
        n = lua_gettop(L) + n + 1;

    t = lua_type(L, n);
    switch (t) {
    case LUA_TBOOLEAN:
        fprintf(f, "%s", (lua_toboolean(L, n) ? "true" : "false"));
        break;
    case LUA_TNUMBER:
        fprintf(f, "%.0f", lua_tonumber(L, n));
        break;
    case LUA_TSTRING:
        fprintf(f, "\"%s\"", lua_tostring(L, n));
        break;
    case LUA_TTABLE:
        {
            int first;

            fprintf(f, "{");

            lua_checkstack(L, 3);
            lua_pushnil(L);  /* first key */
            first = 1;
            while (lua_next(L, n) != 0) {
                /* uses 'key' (at index -2) and 'value' (at index -1) */

                if (!first)
                    fprintf(f, ",");

                fdump(f, L, -2);
                fprintf(f, ":");
                fdump(f, L, -1);

                /* removes 'value'; keeps 'key' for next iteration */
                lua_pop(L, 1);
                first = 0;
            }
            fprintf(f, "}");
        }
        break;
    case LUA_TNIL:
        fprintf(f, "null");
        break;
    case LUA_TFUNCTION:
        fprintf(f, "function@");
        /* fallthrough */
    default:
        fprintf(f, "%p", lua_topointer(L, n));
    }
}
#endif

/*****************************************************************************
 * Function registration
 ****************************************************************************/

static int
lsxe_table_hook_get(lua_State *L)   /* params: (table, field) */
{
    const char *field = luaL_checkstring(L, 2);

    SXEE91("(field=%s)", field);
    SXEA11(lua_type(L, 1) == LUA_TTABLE, "arg(1) is not a table: it's a %s", luaL_typename(L, 1));

    lua_getmetatable(L, 1);
    lua_pushfstring(L, "get_%s", field);
    lua_rawget(L, -2);
    lua_remove(L, -2);

    if (lua_type(L, -1) == LUA_TFUNCTION) {
        SXEL91("found getter get_%s, calling it", field);
        lua_call(L, 0, 1); /* call function with zero args and expect one return */
    }
    else {
        SXEL92("found no getter named get_%s: type is %s", field, luaL_typename(L, -1));
    }

    SXER90("return 1");
    return 1;
}

static int
lsxe_table_hook_set(lua_State *L)   /* params: (table, field, value) */
{
    const char *field = luaL_checkstring(L, 2);

    SXEE91("(field=%s)", field);

    lua_getmetatable(L, 1);
    lua_pushfstring(L, "set_%s", field);
    lua_rawget(L, -2);
    lua_remove(L, -2);

    if (lua_type(L, -1) == LUA_TFUNCTION) {
        SXEL91("found setter set_%s, calling it", field);
        lua_pushvalue(L, 3); /* push copy of the value */
        lua_call(L, 1, 0); /* call function with one arg and expect zero return */
    }
    else {
        SXEL92("found no setter name set_%s: type is %s", field, luaL_typename(L, -1));
    }

    SXER90("return 0");
    return 0;
}

static void
sxe_lua_table_get_metatable(lua_State *L, const char *table)
{
    lua_getglobal(L, table);
    if (lua_type(L, -1) == LUA_TNIL) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, table);
        lua_getglobal(L, table);
    }

    SXEA12(lua_type(L, -1) == LUA_TTABLE, "sxe_lua_table_get_metatable: global '%s' has type %s -- not a table", table, luaL_typename(L, -1));

    /* If the table doesn't have a metatable, create one and set the __index
     * and __newindex fields */
    if (!lua_getmetatable(L, -1)) {
        lua_newtable(L);                    /* S: T M */
        lua_pushvalue(L, -1);               /* S: T M M */
        lua_setmetatable(L, -3);            /* S: T M */
        lua_pushcfunction(L, lsxe_table_hook_get);
        lua_setfield(L, -2, "__index");
        lua_pushcfunction(L, lsxe_table_hook_set);
        lua_setfield(L, -2, "__newindex");
    }

    lua_remove(L, -2);

    /* returns, leaving the metatable on the stack */
}

void
sxe_lua_hook_field(lua_State *L, const char *table, const char *field, lua_CFunction get, lua_CFunction set)
{
    sxe_lua_table_get_metatable(L, table);

    lua_pushfstring(L, "get_%s", field);
    lua_pushcfunction(L, get);
    lua_rawset(L, -3);

    lua_pushfstring(L, "set_%s", field);
    lua_pushcfunction(L, set);
    lua_rawset(L, -3);

    lua_pop(L, 1);
}

static int
lsxe_tie_lstring_get(lua_State *L)
{
    char       * s   = lua_touserdata(L, lua_upvalueindex(1));
    lua_Number   len = lua_tonumber(L, lua_upvalueindex(2));

    s[(size_t)len - 1] = '\0'; /* ensure NUL-terminated */
    lua_pushstring(L, s);

    return 1;
}

static int
lsxe_tie_lstring_set(lua_State *L)
{
    char       * buf = lua_touserdata(L, lua_upvalueindex(1));
    lua_Number   buflen = lua_tonumber(L, lua_upvalueindex(2));
    size_t       len;
    const char * str = lua_tolstring(L, 1, &len);
    snprintf(buf, buflen, "%.*s", (int)len, str);
    return 0;
}

static int
lsxe_tie_int_get(lua_State *L)
{
    int *iptr = lua_touserdata(L, lua_upvalueindex(1));
    lua_pushinteger(L, *iptr);
    return 1;
}

static int
lsxe_tie_int_set(lua_State *L)
{
    int *iptr = lua_touserdata(L, lua_upvalueindex(1));
    int  ival = lua_tonumber(L, 1);
    *iptr = ival;
    return 0;
}

static int
lsxe_tie_long_get(lua_State *L)
{
    long *lptr = lua_touserdata(L, lua_upvalueindex(1));
    lua_pushinteger(L, *lptr);
    return 1;
}

static int
lsxe_tie_long_set(lua_State *L)
{
    long *lptr = lua_touserdata(L, lua_upvalueindex(1));
    long  lval = lua_tonumber(L, 1);
    *lptr = lval;
    return 0;
}

static int
lsxe_tie_bool_get(lua_State *L)
{
    int *bptr = lua_touserdata(L, lua_upvalueindex(1));
    lua_pushboolean(L, *bptr);
    return 1;
}

static int
lsxe_tie_bool_set(lua_State *L)
{
    int *bptr = lua_touserdata(L, lua_upvalueindex(1));
    int  bval = lua_toboolean(L, 1);
    *bptr = bval;
    return 0;
}

void
sxe_lua_tie_field(lua_State *L, const char *table, const char *field, char type, ...)
{
    va_list ap;

    sxe_lua_table_get_metatable(L, table);
    lua_pushfstring(L, "set_%s", field);
    lua_pushfstring(L, "get_%s", field);

    va_start(ap, type);
    switch (type) {
        case 'b':
        {
            int *bptr = va_arg(ap, int *);

            lua_pushlightuserdata(L, bptr);
            lua_pushcclosure(L, lsxe_tie_bool_get, 1);
            lua_rawset(L, -4);

            lua_pushlightuserdata(L, bptr);
            lua_pushcclosure(L, lsxe_tie_bool_set, 1);
            lua_rawset(L, -3);
            break;
        }

        case 's':
        {
            char *buf     = va_arg(ap, char *);
            size_t buflen = va_arg(ap, size_t);

            lua_pushlightuserdata(L, buf);
            lua_pushinteger(L, buflen);
            lua_pushcclosure(L, lsxe_tie_lstring_get, 2);
            lua_rawset(L, -4);

            lua_pushlightuserdata(L, buf);
            lua_pushinteger(L, buflen);
            lua_pushcclosure(L, lsxe_tie_lstring_set, 2);
            lua_rawset(L, -3);
            break;
        }

        case 'i':
        {
            int *iptr = va_arg(ap, int *);

            lua_pushlightuserdata(L, iptr);
            lua_pushcclosure(L, lsxe_tie_int_get, 1);
            lua_rawset(L, -4);

            lua_pushlightuserdata(L, iptr);
            lua_pushcclosure(L, lsxe_tie_int_set, 1);
            lua_rawset(L, -3);
            break;
        }

        case 'l':
        {
            long *lptr = va_arg(ap, long *);

            lua_pushlightuserdata(L, lptr);
            lua_pushcclosure(L, lsxe_tie_long_get, 1);
            lua_rawset(L, -4);

            lua_pushlightuserdata(L, lptr);
            lua_pushcclosure(L, lsxe_tie_long_set, 1);
            lua_rawset(L, -3);
            break;
        }

        default:
            SXEA11(0, "sxe_lua_tie_field(): unsupported data type %c", type); /* coverage exclusion: assertion */
            break;
    }
    va_end(ap);

    lua_pop(L, 1);
}

static int
lsxe_get_log_level(lua_State *L)
{
    int level = sxe_log_get_level();
    lua_pushinteger(L, level);
    SXEL91("lsxe_get_log_level() // returning %d", level);
    return 1;
}

static int
lsxe_set_log_level(lua_State *L)
{
    int level = luaL_checkint(L, 1);
    SXEL91("lsxe_set_log_level(%d)", level);
    sxe_log_set_level(level);
    return 0;
}

static void
register_sxe(lua_State *L)
{
    int i;

    for (i = 0; lsxe_sxe_functions[i].name; i++)
        lua_register(L, lsxe_sxe_functions[i].name, lsxe_sxe_functions[i].func);

    lua_register(L, "SXEL1", lsxe_SXEL1);
    lua_register(L, "SXEL2", lsxe_SXEL2);
    lua_register(L, "SXEL3", lsxe_SXEL3);
    lua_register(L, "SXEL4", lsxe_SXEL4);
    lua_register(L, "SXEL5", lsxe_SXEL5);
    lua_register(L, "SXEL6", lsxe_SXEL6);
    lua_register(L, "SXEL7", lsxe_SXEL7);
    lua_register(L, "SXEL8", lsxe_SXEL8);
    lua_register(L, "SXEL9", lsxe_SXEL9);

    sxe_lua_hook_field(L, "sxe", "log_level", lsxe_get_log_level, lsxe_set_log_level);
}

/*****************************************************************************
 * API
 ****************************************************************************/

SXE_LUA *
sxe_lua_new()
{
    lua_State *L;

    L = luaL_newstate();
    luaL_openlibs(L);

    lua_createtable(L, 0, 0);
    lua_pushvalue(L, -1);
    lua_pushcclosure(L, lsxe_register, 1);
    lua_setglobal(L, "sxe_register");

    register_sxe(L);
    register_sxe_tap(L);
    register_sxe_buf(L);
    register_sxe_pool(L);
    register_sxe_timer(L);

    return L;
}

void
sxe_lua_load(SXE_LUA *L, const char *file)
{
    if (luaL_dofile(L, file) != 0) {
        SXEL32("failed to load script %s: %s", file, lua_tostring(L, -1)); /* coverage exclusion: todo */
    }
}

void
sxe_lua_init(SXE_LUA *L)
{
    initialize_plugins(L);
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
