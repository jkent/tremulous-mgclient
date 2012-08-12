#include "cl_lua.h"

qboolean cl_luaPrintf = qfalse;


/*
======================
CL_LuaWriteString
======================
*/
void CL_LuaWriteString( void *p, const char *s, size_t l )
{
	lua_State *L = (lua_State *)p;
	luaL_Buffer B;

	if (L == cl_luaMasterData.L) {
		CL_LuaPrintf("%s", s);
		return;
	}

	lua_getfield(L, LUA_REGISTRYINDEX, "print");
	if (lua_isnil(L, -1)) {
		lua_pop(L, -1);
		lua_pushlstring(L, s, l);
	}
	else if (lua_isstring(L, -1)) {
		luaL_buffinit(L, &B);
		luaL_addvalue(&B);
		luaL_addlstring(&B, s, l);
		luaL_pushresult(&B);
	}
	lua_setfield(L, LUA_REGISTRYINDEX, "print");
}

/*
======================
CL_LuaWriteLine
======================
*/
void CL_LuaWriteLine( void *p )
{
	lua_State *L = (lua_State *)p;

	if (L == cl_luaMasterData.L) {
		CL_LuaPrintf("\n");
		return;
	}

	lua_getglobal(L, "queue");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	lua_getfield(L, -1, "write");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		return;
	}

	lua_createtable(L, 0, 2);
	lua_pushliteral(L, "print");
	lua_setfield(L, -2, "cmd");
	lua_getfield(L, LUA_REGISTRYINDEX, "print");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_pushliteral(L, "");
	}
	lua_setfield(L, -2, "str");
	lua_call(L, 1, 0);
	lua_pop(L, 1);
}
