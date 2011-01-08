#ifndef __UTILS_H__
#define __UTILS_H__

#define THROW(...) do { lua_pushfstring(L, __VA_ARGS__); lua_error(L); } while (0)
#define \
    lsxe_checkfunction(L, idx) do { \
    if (LUA_TFUNCTION != lua_type(L, idx)) THROW("invalid argument " #idx ": expected function, got %s", luaL_typename(L, idx)); } while (0)

#endif
