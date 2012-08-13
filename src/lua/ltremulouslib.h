#ifndef _ltremulouslib_h
#define _ltremulouslib_h

#include "lua.h"

#define LUA_TREMULOUSLIBNAME	"tremulous"
LUALIB_API int (luaopen_tremulous) (lua_State *L);

#endif /* _ltremulouslib_h */
