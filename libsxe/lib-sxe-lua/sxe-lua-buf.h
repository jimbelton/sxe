#ifndef __LUA_SXE_BUF_H__
#define __LUA_SXE_BUF_H__

void    register_sxe_buf(lua_State *);

/* Wrap an 'SXE' buf in a Lua object. NOTE: the SXE buf must stay referenced
 * while the Lua code uses the object. This is usually not a problem, in
 * practice, because SXE doesn't ever free things -- but the buffer may be
 * reused for another connection, in which case this object will also refer to
 * the buffer being used by the new connection. In any case, be careful. */
int     lsxe_buf_new_from_sxe(lua_State *, SXE *);

/* Check whether the specified argument is a buf object, and return the
 * pointer and length. */
const char *
        lsxe_checkbuf(lua_State *, int, size_t *);

#endif
/* vim: set ft=c sw=4 sts=4 ts=8 listchars=tab\:^.,trail\:@ expandtab list: */
