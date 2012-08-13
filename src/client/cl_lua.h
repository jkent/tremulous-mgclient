#ifndef _CL_LUA_H
#define _CL_LUA_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"

#ifdef USE_LOCAL_HEADERS
#	include "SDL.h"
#	include "SDL_thread.h"
#else
#	include <SDL.h>
#	include <SDL_thread.h>
#endif

/* cl_lua_master_init.lua */

extern unsigned char cl_luaMasterInit[];
extern size_t cl_luaMasterInit_size;


/* cl_lua.c */

struct cl_luaMasterData_t {
	lua_State *L;
	qboolean printing;
	qboolean execing;
};

extern struct cl_luaMasterData_t cl_luaMasterData;

void CL_LuaInit( void );
void CL_LuaShutdown( qboolean full );
void CL_LuaRestart( void );
qboolean CL_LuaCommandHook( void );
void CL_LuaConsoleHook( const char *text );
void CL_LuaFrameHook( void );


/* cl_lua_print.c */

#define CL_LuaPrintf(...) { \
		cl_luaMasterData.printing = qtrue; \
		Com_Printf(__VA_ARGS__); \
		cl_luaMasterData.printing = qfalse; \
}

void CL_LuaWriteString( void *p, const char *s, size_t l );
void CL_LuaWriteLine( void *p );


/* cl_lua_slave_init.lua */

extern unsigned char cl_luaSlaveInit[];
extern size_t cl_luaSlaveInit_size;


/* cl_lua_slave.c */

struct cl_luaSlaveData_t {
	lua_State *L;
	volatile qboolean stop;
	SDL_Thread *thread;
};

qboolean CL_LuaSlaveStart( void );
qboolean CL_LuaSlaveStop( void );


#endif /* _CL_LUA_H */
