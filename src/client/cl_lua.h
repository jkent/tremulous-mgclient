#ifndef _CL_LUA_H
#define _CL_LUA_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"


/* cl_lua_main_init.lua */

extern unsigned char cl_luaMainInit[];
extern size_t cl_luaMainInit_size;


/* cl_lua.c */

struct cl_luaMainData_t {
	lua_State *L;
};

extern qboolean cl_luaCmdExec;

void CL_LuaInit(void);
void CL_LuaShutdown(void);
void CL_LuaRestart(void);
qboolean CL_LuaLoadFile(const char *filename, int rets);
void CL_LuaConsoleHook(const char *text);
qboolean CL_LuaCommandHook(void);


/* cl_lua_print.c */

#define CL_LuaPrintf(...) { \
		cl_luaPrintf = qtrue; \
		Com_Printf(__VA_ARGS__); \
		cl_luaPrintf = qfalse; \
}

extern qboolean cl_luaPrintf;

void CL_LuaWriteString( const char *s, size_t l );
void CL_LuaWriteLine( void );

#endif /* _CL_LUA_H */
