#include "cl_lua.h"
#include "../lua/lqueuelib.h"

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
	{NULL, NULL}
};

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
		CL_LuaPrintf("CL_LuaInit: loadbuffer: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
		lua_close(self->L);
		self->L = NULL;
		return qtrue;
	}

	if ( lua_pcall(self->L, 0, 0, 0) ) {
		CL_LuaPrintf("Lua error: %s\n",
				lua_tostring(self->L, -1));
		lua_pop(self->L, 1);
		lua_close(self->L);
		self->L = NULL;
		return qtrue;
	}

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

	CL_LuaSlaveStartup(self);

	self->stop = qfalse;
	while ( !self->stop ) {
		SDL_Delay(10);
		/* fetch work unit from queue */

		/*
		if ( !workunit ) {
			SDL_Delay(10);
			continue;
		}
		*/

		/* throw work unit at Lua state */
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
