#include "cl_lua.h"

static lua_State *cl_luaState = NULL;
static qboolean cl_luaPrintf = qfalse;
qboolean cl_luaCmdExec = qfalse;

static const char cl_luaRegkeyHook = 0;
static const char cl_luaRegkeyCmd = 0;

#define CL_LUA_WRITEBUF_SIZE 4096
static char cl_luaWriteBuf[CL_LUA_WRITEBUF_SIZE];
static char *cl_luaWritePtr = cl_luaWriteBuf;

#define CL_LuaPrintf(...) { \
		cl_luaPrintf = qtrue; \
		Com_Printf(__VA_ARGS__); \
		cl_luaPrintf = qfalse; \
}

static const char *cl_luaInitScript =
		"local base = trem.cvar_get('fs_basepath').string\n"
		"local home = trem.cvar_get('fs_homepath').string\n"
		"local ext = string.match(package.cpath, '%.(%a+)$')\n"
		"package.path = home .. '/lua/?.lua;' .. "
		  "home .. '/lua/?/init.lua;' .. "
		  "base .. '/lua/?.lua;' .. "
		  "base .. '/lua/?/init.lua'\n"
		"package.cpath = home .. '/lua/?.' .. ext .. ';' .. "
		  "home .. '/lua/loadall.' .. ext .. ';' .. "
		  "base .. '/lua/?.' .. ext .. ';' .. "
		  "base .. '/lua/loadall.' .. ext\n"
		"base, home, ext = nil\n"
		"local fn = package.searchpath('autoexec', package.path)\n "
		"if fn then\n"
		  "return dofile(fn)\n"
		"end\n"
		"print('CL_LuaInit: autoexec not found')\n"
		"print('CL_LuaInit: path: ' .. package.path)\n";

/*
======================
CL_LuaWriteString
======================
*/
void CL_LuaWriteString( const char *s, size_t l )
{
	if ( cl_luaWritePtr - cl_luaWriteBuf + l + 2 < CL_LUA_WRITEBUF_SIZE ) {
		memcpy(cl_luaWritePtr, s, l);
		cl_luaWritePtr += l;
	}
}

/*
======================
CL_LuaWriteLine
======================
*/
void CL_LuaWriteLine( void )
{
	*cl_luaWritePtr++ = '\n';
	*cl_luaWritePtr = '\0';
	cl_luaWritePtr = cl_luaWriteBuf;
	CL_LuaPrintf(cl_luaWriteBuf);
}

/*
======================
CL_Lua_f
======================
*/
void CL_Lua_f( void )
{
	if ( Cmd_Argc() == 2 && !Q_stricmp(Cmd_Argv(1), "restart") ) {
		CL_LuaRestart();
		return;
	}
	else if ( !Q_stricmp(Cmd_Argv(1), "eval") ) {
		if ( luaL_loadstring(cl_luaState, Cmd_Cmd() + 9) ) {
			CL_LuaPrintf("eval: loadstring: %s\n",
					lua_tostring(cl_luaState, -1));
			lua_pop(cl_luaState, 1);
			return;
		}

		if ( lua_pcall(cl_luaState, 0, 0, 0) ) {
			CL_LuaPrintf("eval: pcall: %s\n",
					lua_tostring(cl_luaState, -1));
			lua_pop(cl_luaState, 1);
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

	cl_luaState = luaL_newstate();

	if ( !cl_luaState ) {
		Com_Error(ERR_FATAL, "CL_LuaInit: failed\n");
	}

	luaL_openlibs(cl_luaState);

	lua_getglobal(cl_luaState, "trem");
	lua_newtable(cl_luaState);
	lua_newtable(cl_luaState);

	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyHook);
	lua_pushvalue(cl_luaState, 3);
	lua_settable(cl_luaState, LUA_REGISTRYINDEX);

	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyCmd);
	lua_pushvalue(cl_luaState, 2);
	lua_settable(cl_luaState, LUA_REGISTRYINDEX);

	lua_setfield(cl_luaState, 1, "hook");
	lua_setfield(cl_luaState, 1, "cmd");
	lua_pop(cl_luaState, 1);

	Cmd_AddCommand("lua", CL_Lua_f);

	if ( luaL_loadbuffer(cl_luaState, cl_luaInitScript,
			strlen(cl_luaInitScript), "init") ) {
		CL_LuaPrintf("CL_LuaInit: loadbuffer: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
		return;
	}

	if ( lua_pcall(cl_luaState, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
	}

	lua_gc(cl_luaState, LUA_GCCOLLECT, 0);
}

/*
======================
CL_LuaShutdown
======================
*/
void CL_LuaShutdown( void )
{
	Cmd_RemoveCommand("lua");

	if ( cl_luaState ) {
		lua_close(cl_luaState);
		cl_luaState = NULL;
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
CL_LuaConsoleHook
======================
*/
void CL_LuaConsoleHook( const char *text )
{
	luaL_Buffer b;
	int len;
	char *s;

	if ( cl_luaPrintf || !cl_luaState ) {
		return;
	}

	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyHook);
	lua_gettable(cl_luaState, LUA_REGISTRYINDEX);
	lua_pushliteral(cl_luaState, "console");
	lua_gettable(cl_luaState, -2);
	if ( !lua_isfunction(cl_luaState, -1) ) {
		lua_pop(cl_luaState, 2);
		return;
	}
	lua_remove(cl_luaState, 1);

	len = strlen(text);
	if ( text[len - 1] == '\n' ) {
		len -= 1;
	}
	s = luaL_buffinitsize(cl_luaState, &b, len);
	memcpy(s, text, len);
	luaL_pushresultsize(&b, len);

	if ( lua_pcall(cl_luaState, 1, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
	}
}

/*
======================
CL_LuaCommandHook
======================
*/
static int CL_LuaCommandHookCall( void ) /* -2 +0 */
{
	int argc, i;

	if ( !lua_isfunction(cl_luaState, -1) ) {
		lua_pop(cl_luaState, 2);
		return -1;
	}

	argc = Cmd_Argc();
	lua_createtable(cl_luaState, argc, 0);
	for ( i = 0; i < argc; i++ ) {
		lua_pushstring(cl_luaState, Cmd_Argv(i));
		lua_rawseti(cl_luaState, -2, i + 1);
	}
	lua_pushstring(cl_luaState, Cmd_Cmd());

	if ( lua_pcall(cl_luaState, 2, 1, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 2);
		return -2;
	}

	return lua_toboolean(cl_luaState, -1) ? 1 : 0;
}

qboolean CL_LuaCommandHook( void )
{
	int n;

	if ( cl_luaCmdExec || !cl_luaState ) {
		return qfalse;
	}

	/* trem.hook.command */
	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyHook);
	lua_gettable(cl_luaState, LUA_REGISTRYINDEX);
	lua_pushliteral(cl_luaState, "command");
	lua_gettable(cl_luaState, -2);
	n = CL_LuaCommandHookCall();
	if ( n == 1 ) {
		return qtrue;
	}

	/* trem.cmd[<arg0>] */
	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyCmd);
	lua_gettable(cl_luaState, LUA_REGISTRYINDEX);
	lua_pushstring(cl_luaState, Cmd_Argv(0));
	lua_gettable(cl_luaState, -2);
	n = CL_LuaCommandHookCall();
	if ( n == 0 ) {
		return qtrue;
	}
	return qfalse;
}
