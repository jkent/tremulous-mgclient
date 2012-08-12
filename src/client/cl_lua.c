#include "cl_lua.h"
#include "../lua/lqueuelib.h"

struct cl_luaMasterData_t cl_luaMasterData = {0};


/*
======================
CL_Lua_f
======================
*/
void CL_Lua_f( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if ( Cmd_Argc() == 2 && !Q_stricmp(Cmd_Argv(1), "restart") ) {
		CL_LuaRestart();
		return;
	}
	else if ( !Q_stricmp(Cmd_Argv(1), "eval") ) {
		// TODO: rework so it runs within slave
		if ( luaL_loadstring(self->L, Cmd_Cmd() + 9) ) {
			CL_LuaPrintf("eval: loadstring: %s\n",
					lua_tostring(self->L, -1));
			lua_pop(self->L, 1);
			return;
		}

		if ( lua_pcall(self->L, 0, 0, 0) ) {
			CL_LuaPrintf("eval: pcall: %s\n",
					lua_tostring(self->L, -1));
			lua_pop(self->L, 1);
			return;
		}
		return;
	}

	CL_LuaPrintf("lua [ restart | eval code ]\n");
}

/*
======================
CL_LuaInit
======================
*/
void CL_LuaInit( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if (self->L) {
		Com_Printf("CL_LuaInit: already initialized\n");
		return;
	}

	self->L = luaL_newstate();
	if ( !self->L ) {
		Com_Printf("CL_LuaInit: failed\n");
	}

	luaL_openlibs(self->L);

	if ( CL_LuaSlaveStart() ) {
		CL_LuaShutdown();
		return;
	}

	Cmd_AddCommand("lua", CL_Lua_f);

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
void CL_LuaShutdown( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	CL_LuaSlaveStop();

	Cmd_RemoveCommand("lua");

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
	CL_LuaShutdown();
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

	argc = Cmd_Argc();
	lua_createtable(self->L, argc, 0);
	for ( i = 0; i < argc; i++ ) {
		lua_pushstring(self->L, Cmd_Argv(i));
		lua_rawseti(self->L, -2, i + 1);
	}
	lua_pushstring(self->L, Cmd_Cmd());

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
CL_LuaConsoleHook
======================
*/
void CL_LuaConsoleHook( const char *text )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	luaL_Buffer b;
	int len;
	char *s;

	if ( self->printing || !self->L ) {
		return;
	}

	lua_getglobal(self->L, "console_hook");
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 1);
		return;
	}

	len = strlen(text);
	if ( text[len - 1] == '\n' ) {
		len -= 1;
	}
	s = luaL_buffinitsize(self->L, &b, len);
	memcpy(s, text, len);
	luaL_pushresultsize(&b, len);

	if ( lua_pcall(self->L, 1, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}
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
