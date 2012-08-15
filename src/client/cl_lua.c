#include "cl_lua.h"
#include "../lua/lqueuelib.h"
#include "../lua/ltremulouslib.h"

struct cl_luaMasterData_t cl_luaMasterData = {0};

static const luaL_Reg loadedlibs[] = {
	{"_G", luaopen_base},
	{LUA_QUEUELIBNAME, luaopen_queue},
	{LUA_TREMULOUSLIBNAME, luaopen_tremulous},
	{NULL, NULL}
};


/*
======================
CL_Lua_f
======================
*/
void CL_Lua_f( void )
{
	if ( Cmd_Argc() == 2 ) {
		if ( !Q_stricmp(Cmd_Argv(1), "start") ) {
			CL_LuaInit();
			return;
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "stop") ) {
			CL_LuaShutdown( qfalse );
			return;
		}
		else if ( !Q_stricmp(Cmd_Argv(1), "restart") ) {
			CL_LuaRestart();
			return;
		}
	}

	CL_LuaPrintf("lua [start|stop|restart]\n");
}

/*
======================
CL_LuaInit
======================
*/
void CL_LuaInit( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;
	const luaL_Reg *lib;

	if (self->L) {
		Com_Printf("CL_LuaInit: already initialized\n");
		return;
	}

	Cmd_RemoveCommand("lua");
	Cmd_AddCommand("lua", CL_Lua_f);

	self->L = luaL_newstate();
	if ( !self->L ) {
		Com_Printf("CL_LuaInit: failed\n");
	}

	for (lib = loadedlibs; lib->func; lib++) {
		luaL_requiref(self->L, lib->name, lib->func, 1);
		lua_pop(self->L, 1);
	}

	if ( CL_LuaSlaveStart() ) {
		CL_LuaShutdown( qfalse );
		return;
	}

	if ( luaL_loadbuffer(self->L, (const char *) cl_luaMasterInit,
			cl_luaMasterInit_size, "master_init") ) {
		CL_LuaPrintf("CL_LuaInit: loadbuffer: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
		return;
	}

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}

	lua_gc(self->L, LUA_GCCOLLECT, 0);
}

/*
======================
CL_LuaShutdown
======================
*/
void CL_LuaShutdown( qboolean full )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	CL_LuaSlaveStop();

	if ( full ) {
		Cmd_RemoveCommand("lua");
	}

	if ( self->L ) {
		luaclose_queue(self->L);
		lua_close(self->L);
		self->L = NULL;
	}
}

/*
======================
CL_LuaRestart
======================
*/
void CL_LuaRestart( void )
{
	CL_LuaShutdown( qfalse );
	CL_LuaInit();
}

/*
======================
CL_LuaCommandHook
======================
*/
qboolean CL_LuaCommandHook( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	int argc, i;

	if ( self->execing || !self->L ) {
		return qfalse;
	}

	lua_getglobal(self->L, "command_hook");
	if (!lua_isfunction(self->L, -1)) {
		lua_pop(self->L, 1);
		return qfalse;
	}

	lua_pushstring(self->L, Cmd_Cmd());
	argc = Cmd_Argc();
	lua_createtable(self->L, argc, 0);
	for ( i = 0; i < argc; i++ ) {
		lua_pushstring(self->L, Cmd_Argv(i));
		lua_rawseti(self->L, -2, i + 1);
	}

	if ( lua_pcall(self->L, 2, 1, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
		return qfalse;
	}

	return lua_toboolean(self->L, -1) ? qtrue : qfalse;
}

/*
======================
CL_LuaConnectHook
======================
*/
void CL_LuaConnectHook( const char *addr )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if ( !self->L ) {
		return;
	}

	lua_getglobal(self->L, "connect_hook");
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 1);
		return;
	}

	lua_pushstring(self->L, addr);
	if ( lua_pcall(self->L, 1, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}

	return;
}

/*
======================
CL_LuaDisconnectHook
======================
*/
void CL_LuaDisconnectHook( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if ( !self->L ) {
		return;
	}

	lua_getglobal(self->L, "disconnect_hook");
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 1);
		return;
	}

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}

	return;
}

/*
======================
CL_LuaFrameHook
======================
*/
void CL_LuaFrameHook( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if ( !self->L ) {
		return;
	}

	lua_getglobal(self->L, "frame_hook");
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 1);
		return;
	}

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}
}

/*
======================
CL_LuaPrintHook
======================
*/
qboolean CL_LuaPrintHook( const char *text )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	luaL_Buffer B;
	int len;
	char *s;

	if ( self->printing || !self->L ) {
		return qfalse;
	}

	lua_getglobal(self->L, "print_hook");
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 1);
		return qfalse;
	}

	len = strlen(text);
	if ( text[len - 1] == '\n' ) {
		len -= 1;
	}
	s = luaL_buffinitsize(self->L, &B, len);
	memcpy(s, text, len);
	luaL_pushresultsize(&B, len);

	if ( lua_pcall(self->L, 1, 1, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
		return qfalse;
	}

	return lua_toboolean(self->L, -1) ? qtrue : qfalse;
}
