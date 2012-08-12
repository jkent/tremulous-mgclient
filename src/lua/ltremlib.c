/* vim: set et ts=2: */

#include "ltremlib.h"
#include "lauxlib.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/cl_lua.h"

static int ltrem_exec( lua_State *L )
{
	struct cl_luaMasterData_t *master = &cl_luaMasterData;
	luaL_Buffer B;
	int argc = lua_gettop(L);
	int i;

	if ( master->L != L || !lua_isstring(L, -1) ) {
		return 0;
	}

	luaL_buffinit(L, &B);
	lua_pushvalue(L, 1);
	luaL_addvalue(&B);

	for ( i = 2; i <= argc; i++ ) {
		luaL_addstring(&B, " \"");
		if ( lua_isstring(L, i) ) {
			lua_pushvalue(L, i);
			luaL_addvalue(&B);
		}
		luaL_addchar(&B, '"');
	}
	luaL_addchar(&B, '\n');
	luaL_pushresult(&B);

	master->execing = qtrue;
	Cmd_ExecuteString(lua_tostring(L, -1));
	master->execing = qfalse;
	return 0;
}

static int ltrem_get_cvar( lua_State *L )
{
	const char *name, *value;

	if ( !lua_isstring(L, 1) ) {
		lua_pushnil(L);
		return 1;
	}

	name = lua_tostring(L, 1);
	value = Cvar_VariableString(name);

	if ( strcmp("", value) == 0 ) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, value);
	return 1;
}

cvar_t *Cvar_Set2( const char *var_name, const char *value, qboolean force );
static int ltrem_set_cvar( lua_State *L )
{
	const char *name, *value;

	if ( !lua_isstring(L, 1) ) {
		return 0;
	}
	name = lua_tostring(L, 1);

	if ( lua_isnil(L, 2) ) {
		Cvar_Reset(name);
		return 0;
	}

	value = lua_tostring(L, 2);
	if ( !value ) {
		return 0;
	}

	Cvar_Set2(name, value, qfalse);

	return 0;
}

static const luaL_Reg tremlib[] = {
	{"exec", ltrem_exec},
	{"get_cvar", ltrem_get_cvar},
	{"set_cvar", ltrem_set_cvar},
	{NULL, NULL}
};

LUALIB_API int luaopen_trem( lua_State *L )
{
	luaL_newlib(L, tremlib);
	return 1;
}
