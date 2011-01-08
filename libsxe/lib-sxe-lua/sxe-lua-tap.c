#include "sxe.h"
#include "sxe-util.h"
#include "tap.h"

#include "lua.h"
#include "ltm.h"
#include "lauxlib.h"
#include "lualib.h"

#include "sxe-lua-tap.h"

#define dINFO \
    lua_Debug ar;const char *code,*func,*file;int line

static void
getinfo(lua_State *L, lua_Debug *ar, const char **func,
        const char **file, int *line, const char **code)
{
    *func = "<main>";
    *file = "<file>";
    *line = -1;
    *code = "????";
    if (!lua_getstack(L, 1, ar))
        luaL_error(L, "error getting stack frame");    /* COVERAGE EXCLUSION: have no idea when this happens */
    lua_getinfo(L, "Snl", ar);
    if (*ar->namewhat)
        *func = ar->name;                              /* COVERAGE EXCLUSION: have no idea when this happens */
    if (ar->currentline > 0) {
        *file = ar->short_src;
        *line = ar->currentline;
    }
}

static int
lsxe_tap_plan_tests(lua_State *L)
{
    unsigned plan = luaL_checkint(L, 1);
    plan_tests(plan);
    return 0;
}

static int
lsxe_tap_plan_no_plan(lua_State *L)      /* COVERAGE EXCLUSION: we already planned some tests! */
{                                        /* COVERAGE EXCLUSION: we already planned some tests! */
    SXE_UNUSED_PARAMETER(L);             /* COVERAGE EXCLUSION: we already planned some tests! */
    plan_no_plan();                      /* COVERAGE EXCLUSION: we already planned some tests! */
    return 0;                            /* COVERAGE EXCLUSION: we already planned some tests! */
}

static int
lsxe_tap_diag(lua_State *L)
{
    const char *msg = luaL_checkstring(L, 1);
    diag("%s", msg);
    return 0;
}

static int
lsxe_tap_pass(lua_State *L)
{
    dINFO;
    const char *msg = luaL_checkstring(L, 1);
    getinfo(L,&ar,&func,&file,&line,&code);
    _gen_result(0,(const void*)1,0,0,0,func,file,line,"%s",msg);
    return 0;
}

static int
lsxe_tap_fail(lua_State *L)                                         /* COVERAGE EXCLUSION: don't test failures */
{                                                                   /* COVERAGE EXCLUSION: don't test failures */
    dINFO;                                                          /* COVERAGE EXCLUSION: don't test failures */
    const char *msg = luaL_checkstring(L, 1);                       /* COVERAGE EXCLUSION: don't test failures */
    getinfo(L,&ar,&func,&file,&line,&code);                         /* COVERAGE EXCLUSION: don't test failures */
    _gen_result(0,(const void*)0,0,0,0,func,file,line,"%s",msg);    /* COVERAGE EXCLUSION: don't test failures */
    return 0;                                                       /* COVERAGE EXCLUSION: don't test failures */
}

static int
lsxe_tap_ok1(lua_State *L)
{
    dINFO;
    int test = lua_toboolean(L, 1) ? 1 : 0;

    getinfo(L,&ar,&func,&file,&line,&code);
    _gen_result(0,(const void*)test,0,0,0,func,file,line,"%s",code);
    return 0;
}

static int
lsxe_tap_ok(lua_State *L)
{
    dINFO;
    int          test = lua_toboolean(L, 1) ? 1 : 0;
    const char * msg  = luaL_checkstring(L, 2);

    getinfo(L,&ar,&func,&file,&line,&code);
    _gen_result(0,(const void*)test,0,0,0,func,file,line,"%s",msg);
    return 0;
}

static int
lsxe_tap_is(lua_State *L)
{
    dINFO;
    int          test = lua_equal(L, 1, 2) ? 1 : 0;
    const char * msg  = lua_gettop(L) == 3 ? luaL_checkstring(L, 3) : "unknown test";

    getinfo(L,&ar,&func,&file,&line,&code);
    _gen_result(0,(const void*)test,0,0,0,func,file,line,"%s",msg);
    return 0;
}

static const luaL_Reg lsxe_tap_functions[] = {
    {"plan_tests",      lsxe_tap_plan_tests},
    {"plan_no_plan",    lsxe_tap_plan_no_plan},
    {"pass",            lsxe_tap_pass},
    {"fail",            lsxe_tap_fail},
    {"diag",            lsxe_tap_diag},
    {"ok1",             lsxe_tap_ok1},
    {"ok",              lsxe_tap_ok},
    {"is",              lsxe_tap_is},

    {NULL, NULL}
};

/*****************************************************************************
 * Function registration
 ****************************************************************************/

void
register_sxe_tap(lua_State *L)
{
    int i;
    for (i = 0; lsxe_tap_functions[i].name; i++)
        lua_register(L, lsxe_tap_functions[i].name, lsxe_tap_functions[i].func);
}

/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
