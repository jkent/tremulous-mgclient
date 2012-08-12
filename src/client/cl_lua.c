#include "cl_lua.h"
#include "../lua/lqueuelib.h"

struct cl_luaMasterData_t cl_luaMasterData = {0};
qboolean cl_luaCmdExec = qfalse;

static const char cl_luaRegkeyHook = 0;
static const char cl_luaRegkeyCmd = 0;


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
		lua_close(self->L);
		self->L = NULL;
		return;
	}

	lua_getglobal(self->L, "trem");
	lua_newtable(self->L);
	lua_newtable(self->L);

	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyHook);
	lua_pushvalue(self->L, 3);
	lua_settable(self->L, LUA_REGISTRYINDEX);

	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyCmd);
	lua_pushvalue(self->L, 2);
	lua_settable(self->L, LUA_REGISTRYINDEX);

	lua_setfield(self->L, 1, "hook");
	lua_setfield(self->L, 1, "cmd");
	lua_pop(self->L, 1);

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

	luaclose_queue(self->L);

	if ( self->L ) {
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
CL_LuaFrame
======================
*/
void CL_LuaFrame( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	if ( !self->L ) {
		return;
	}

	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyHook);
	lua_gettable(self->L, LUA_REGISTRYINDEX);
	lua_pushliteral(self->L, "frame");
	lua_gettable(self->L, -2);
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 2);
		return;
	}
	lua_remove(self->L, 1);

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
	}
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

	if ( cl_luaPrintf || !self->L ) {
		return;
	}

	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyHook);
	lua_gettable(self->L, LUA_REGISTRYINDEX);
	lua_pushliteral(self->L, "console");
	lua_gettable(self->L, -2);
	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 2);
		return;
	}
	lua_remove(self->L, 1);

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
CL_LuaCommandHook
======================
*/
static int CL_LuaCommandHookCall( void ) /* -2 +0 */
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	int argc, i;

	if ( !lua_isfunction(self->L, -1) ) {
		lua_pop(self->L, 2);
		return -1;
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
		lua_pop(self->L, 2);
		return -2;
	}

	return lua_toboolean(self->L, -1) ? 1 : 0;
}

qboolean CL_LuaCommandHook( void )
{
	struct cl_luaMasterData_t *self = &cl_luaMasterData;

	int n;

	if ( cl_luaCmdExec || !self->L ) {
		return qfalse;
	}

	/* trem.hook.command */
	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyHook);
	lua_gettable(self->L, LUA_REGISTRYINDEX);
	lua_pushliteral(self->L, "command");
	lua_gettable(self->L, -2);
	n = CL_LuaCommandHookCall();
	if ( n == 1 ) {
		return qtrue;
	}

	/* trem.cmd[<arg0>] */
	lua_pushlightuserdata(self->L, (void *) &cl_luaRegkeyCmd);
	lua_gettable(self->L, LUA_REGISTRYINDEX);
	lua_pushstring(self->L, Cmd_Argv(0));
	lua_gettable(self->L, -2);
	n = CL_LuaCommandHookCall();
	if ( n == 0 ) {
		return qtrue;
	}
	return qfalse;
}
