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
	else if ( Cmd_Argc() == 3 && !Q_stricmp(Cmd_Argv(1), "exec") ) {
		CL_LuaLoadFile(Cmd_Argv(2), 0);
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

	CL_LuaPrintf("lua [ restart | exec file | eval code ]\n");
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
	else if ( lua_pcall(cl_luaState, 0, 0, 0) ) {
		CL_LuaPrintf("CL_LuaInit: %s\n", lua_tostring(cl_luaState, -1));
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
CL_LuaLoadFile
======================
*/
qboolean CL_LuaLoadFile( const char *filename, int rets )
{
	int len;
	fileHandle_t f;
	char buf[MAX_LUAFILE];

	if ( !cl_luaState ) {
		return qfalse;
	}

	len = FS_FOpenFileRead(filename, &f, qfalse);
	if ( !f ) {
		CL_LuaPrintf("CL_LuaLoadFile: file not found: %s\n", filename);
		return qfalse;
	}

	if ( len >= MAX_LUAFILE ) {
		CL_LuaPrintf("CL_LuaLoadFile: file too large: %s is %i (max %i)\n",
				filename, len, MAX_LUAFILE);
		FS_FCloseFile(f);
		return qfalse;
	}

	FS_Read(buf, len, f);
	buf[len] = 0;
	FS_FCloseFile(f);

	if ( luaL_loadbuffer(cl_luaState, buf, strlen(buf), filename) ) {
		CL_LuaPrintf("CL_LuaLoadFile: loadbuffer: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
		return qfalse;
	}

	if ( lua_pcall(cl_luaState, 0, rets, 0) ) {
		CL_LuaPrintf("CL_LuaLoadFile: pcall: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
		return qfalse;
	}

	lua_gc(cl_luaState, LUA_GCCOLLECT, 0);

	return qtrue;
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
		CL_LuaPrintf("CL_LuaConsoleHook: trem.hook.console: %s\n",
				lua_tostring(cl_luaState, -1));
		lua_pop(cl_luaState, 1);
	}
}

/*
======================
CL_LuaCommandHook
======================
*/
qboolean CL_LuaCommandHook( void )
{
	int argc, i;

	if ( cl_luaCmdExec || !cl_luaState ) {
		return qfalse;
	}

	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyHook);
	lua_gettable(cl_luaState, LUA_REGISTRYINDEX);
	lua_pushliteral(cl_luaState, "command");
	lua_gettable(cl_luaState, -2);
	if ( lua_isfunction(cl_luaState, -1) ) {
		argc = Cmd_Argc();
		lua_createtable(cl_luaState, argc, 0);
		for ( i = 0; i < argc; i++ ) {
			lua_pushstring(cl_luaState, Cmd_Argv(i));
			lua_rawseti(cl_luaState, -2, i + 1);
		}
		lua_pushstring(cl_luaState, Cmd_Cmd());

		if ( lua_pcall(cl_luaState, 2, 1, 0) ) {
			CL_LuaPrintf("CL_LuaCommandHook: trem.hook[\"command\"]: %s\n",
					lua_tostring(cl_luaState, -1));
		}
		else if ( lua_toboolean(cl_luaState, -1) ) {
			lua_pop(cl_luaState, 2);
			return qtrue;
		}
	}
	lua_pop(cl_luaState, 2);

	lua_pushlightuserdata(cl_luaState, (void *) &cl_luaRegkeyCmd);
	lua_gettable(cl_luaState, LUA_REGISTRYINDEX);
	lua_pushstring(cl_luaState, Cmd_Argv(0));
	lua_gettable(cl_luaState, -2);
	if ( lua_isfunction(cl_luaState, -1) ) {
		argc = Cmd_Argc();
		lua_createtable(cl_luaState, argc, 0);
		for ( i = 0; i < argc; i++ ) {
			lua_pushstring(cl_luaState, Cmd_Argv(i));
			lua_rawseti(cl_luaState, -2, i + 1);
		}
		lua_pushstring(cl_luaState, Cmd_Cmd());

		if ( lua_pcall(cl_luaState, 2, 1, 0) ) {
			CL_LuaPrintf("CL_LuaCommandHook: trem.cmd[\"%s\"]: %s\n",
					Cmd_Argv(0), lua_tostring(cl_luaState, -1));
			lua_pop(cl_luaState, 2);
			return qfalse;
		}

		i = lua_toboolean(cl_luaState, -1);
		lua_pop(cl_luaState, 2);
		return (i == 0) ? qtrue : qfalse;
	}

	lua_pop(cl_luaState, 2);
	return qfalse;
}
