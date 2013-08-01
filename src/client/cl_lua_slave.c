#include "cl_lua.h"
#include "../lua/lqueuelib.h"
#include "../lua/lfs.h"

#ifdef USE_LUASOCKET
#include "../lua/socket/luasocket.h"
#include "../lua/socket/mime.h"
#if defined(LUA_USE_POSIX) || defined(LUA_USE_LINUX)
#include "../lua/socket/unix.h"
#endif
#endif

#ifdef USE_LUASQLITE
#include "../lua/sqlite/lsqlite3.h"
#endif

#ifdef USE_LUAREGEX
#include "../lua/regex/lregex.h"
#endif

static struct cl_luaSlaveData_t cl_luaSlaveData = {0};

static const luaL_Reg loadedlibs[] = {
	{"_G", luaopen_base},
	{LUA_LOADLIBNAME, luaopen_package},
	{LUA_TABLIBNAME, luaopen_table},
	{LUA_STRLIBNAME, luaopen_string},
	{LUA_QUEUELIBNAME, luaopen_queue},
	{NULL, NULL}
};

static const luaL_Reg preloadedlibs[] = {
	{LUA_COLIBNAME, luaopen_coroutine},
	{LUA_IOLIBNAME, luaopen_io},
	{LUA_OSLIBNAME, luaopen_os},
	{LUA_BITLIBNAME, luaopen_bit32},
	{LUA_MATHLIBNAME, luaopen_math},
	{LUA_DBLIBNAME, luaopen_debug},
	{"lfs", luaopen_lfs},
#ifdef USE_LUASOCKET
	{"socket.core", luaopen_socket_core},
	{"mime.core",  luaopen_mime_core},
#if defined(LUA_USE_POSIX) || defined(LUA_USE_LINUX)
	{"socket.unix", luaopen_socket_unix},
#endif
#endif
#ifdef USE_LUASQLITE
	{"sqlite3", luaopen_lsqlite3},
#endif
#ifdef USE_LUAREGEX
	{REX_LIBNAME, REX_OPENLIB},
#endif
	{NULL, NULL}
};

static int CL_LuaSlaveIsStopped (lua_State *L)
{
	struct cl_luaSlaveData_t *self = (struct cl_luaSlaveData_t *) &cl_luaSlaveData;
	lua_pushboolean(L, self->stop == qtrue);
	return 1;
}

static qboolean CL_LuaSlaveStartup( struct cl_luaSlaveData_t *self )
{
	const luaL_Reg *lib;

	self->L = luaL_newstate();
	if ( !self->L ) {
		return qtrue;
	}

	for (lib = loadedlibs; lib->func; lib++) {
		luaL_requiref(self->L, lib->name, lib->func, 1);
		lua_pop(self->L, 1);
	}
	luaL_getsubtable(self->L, LUA_REGISTRYINDEX, "_PRELOAD");
	for (lib = preloadedlibs; lib->func; lib++) {
		lua_pushcfunction(self->L, lib->func);
		lua_setfield(self->L, -2, lib->name);
	}
	lua_pop(self->L, 1);

	if ( luaL_loadbuffer(self->L, (const char *) cl_luaSlaveInit,
			cl_luaSlaveInit_size, "slave_init") ) {
		lua_getglobal(self->L, "print");
		lua_pushfstring(self->L, "CL_LuaInit: loadbuffer: %s", lua_tostring(self->L, -2));
		lua_call(self->L, 1, 0);
		lua_pop(self->L, 1);
		return qtrue;
	}

	if ( lua_pcall(self->L, 0, 1, 0) ) {
		lua_getglobal(self->L, "print");
		lua_pushfstring(self->L, "Lua error: %s", lua_tostring(self->L, -2));
		lua_call(self->L, 1, 0);
		lua_pop(self->L, 1);
		return qtrue;
	}

	if (!lua_isfunction(self->L, -1)) {
		lua_getglobal(self->L, "print");
		lua_pushliteral(self->L, "Lua error: init did not return main function");
		lua_call(self->L, 1, 0);
		lua_pop(self->L, 1);
		return qtrue;
	}

	self->stop = qfalse;
	lua_register(self->L, "stopped", CL_LuaSlaveIsStopped);

	lua_gc(self->L, LUA_GCCOLLECT, 0);
	return qfalse;
}

static void CL_LuaSlaveShutdown( struct cl_luaSlaveData_t *self )
{
	if (self->L) {
		luaclose_queue(self->L);
		lua_close(self->L);
		self->L = NULL;
	}
}

static int CL_LuaSlave( void *arg )
{
	struct cl_luaSlaveData_t *self = (struct cl_luaSlaveData_t *) arg;

	if (CL_LuaSlaveStartup(self)) {
		CL_LuaSlaveShutdown(self);
		return 1;
	}

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		lua_getglobal(self->L, "print");
		lua_pushfstring(self->L, "Lua error: %s", lua_tostring(self->L, -2));
		lua_call(self->L, 1, 0);
		lua_pop(self->L, 1);
	}

	CL_LuaSlaveShutdown(self);

	return 0;
}

qboolean CL_LuaSlaveStart( void )
{
	struct cl_luaSlaveData_t *self = &cl_luaSlaveData;

	if ( self->thread ) {
		return qtrue;
	}

	self->thread = SDL_CreateThread(CL_LuaSlave, &cl_luaSlaveData);
	if ( !self->thread ) {
		return qtrue;
	}

	return qfalse;
}


qboolean CL_LuaSlaveStop( void )
{
	struct cl_luaSlaveData_t *self = &cl_luaSlaveData;

	if ( !self->thread ) {
		return qtrue;
	}

	self->stop = qtrue;
	SDL_WaitThread(self->thread, NULL);
	self->thread = qfalse;

	return qfalse;
}
