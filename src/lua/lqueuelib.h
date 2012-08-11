#ifndef _lqueuelib_h
#define _lqueuelib_h

#include "lua.h"

#define LUA_QUEUELIBNAME "queue"
LUALIB_API int (luaopen_queue) (lua_State *L);
LUALIB_API void (luaclose_queue) (lua_State *L);

#endif /* _lqueuelib_h */
