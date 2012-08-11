#include "cl_lua.h"

qboolean cl_luaPrintf = qfalse;


/*
======================
CL_LuaWriteString
======================
*/
void CL_LuaWriteString( void *p, const char *s )
{
	lua_State *L = (lua_State *)p;

	if (L == cl_luaMasterData.L) {
		CL_LuaPrintf("%s", s);
	}
	else {
		/* communicate */
	}
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
	}
	else {
		/* communicate */
	}
}
