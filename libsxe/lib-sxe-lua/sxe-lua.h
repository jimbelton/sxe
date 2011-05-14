#ifndef __LUA_SXE_H__
#define __LUA_SXE_H__

struct lua_State;
typedef struct \
        lua_State SXE_LUA;

SXE_LUA * sxe_lua_new(void);
void      sxe_lua_load(SXE_LUA *, const char *);
void      sxe_lua_init(SXE_LUA *);

void      sxe_lua_hook_field(SXE_LUA *, const char *, const char *, int (*getter)(SXE_LUA *), int (*setter)(SXE_LUA *));
void      sxe_lua_tie_field(SXE_LUA *, const char *, const char *, char, ...);

#endif
/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
