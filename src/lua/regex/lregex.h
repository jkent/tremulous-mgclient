#ifndef lregex_h
#define lregex_h

#include "../lua.h"

/* These 2 settings may be redefined from the command-line or the makefile.
 * They should be kept in sync between themselves and with the target name.
 */
#ifndef REX_LIBNAME
#  define REX_LIBNAME "rex_pcre"
#endif
#ifndef REX_OPENLIB
#  define REX_OPENLIB luaopen_rex_pcre
#endif

#define REX_TYPENAME REX_LIBNAME"_regex"

LUALIB_API int REX_OPENLIB(lua_State *L);

#endif /* lregex_h */
