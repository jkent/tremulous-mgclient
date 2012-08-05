/* vim: set et ts=2: */

#include "ltremlib.h"
#include "lauxlib.h"
#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../client/cl_lua.h"

static int ltrem_cmd_exec (lua_State *L) {
	luaL_Buffer b;
  int argc = lua_gettop(L);
  int i;

  if (!lua_isstring(L, -1)) {
    return 0;
  }
  luaL_buffinit(L, &b);
  lua_pushvalue(L, 1);
  luaL_addvalue(&b);

  for (i = 2; i <= argc; i++) {
    luaL_addstring(&b, " \"");
    if (lua_isstring(L, i)) {
      lua_pushvalue(L, i);
      luaL_addvalue(&b);
    }
    luaL_addchar(&b, '"');
  }
  luaL_addchar(&b, '\n');
  luaL_pushresult(&b);
  cl_luaCmdExec = qtrue;
  Cmd_ExecuteString(lua_tostring(L, -1));
  cl_luaCmdExec = qfalse;
  return 0;
}

static int ltrem_cvar (lua_State *L, cvar_t *v) {
  if ( !v ) {
    lua_pushnil(L);
    return 1;
  }

  lua_newtable(L);
  lua_pushstring(L, v->string);
  lua_setfield(L, -2, "string");
  lua_pushstring(L, v->resetString);
  lua_setfield(L, -2, "reset_string");
  if ( v->latchedString ) {
    lua_pushstring(L, v->latchedString);
    lua_setfield(L, -2, "latched_string");
  }
  lua_pushinteger(L, v->integer);
  lua_setfield(L, -2, "integer");
  lua_pushnumber(L, v->value);
  lua_setfield(L, -2, "value");
  lua_pushnumber(L, v->flags);
  lua_setfield(L, -2, "flags");
  return 1;
}

cvar_t *Cvar_FindVar( const char *var_name );
static int ltrem_cvar_get (lua_State *L) {
  const char *var_name = lua_tostring(L, 1);
  if (!var_name) {
    lua_pushnil(L);
    return 1;
  }
  return ltrem_cvar(L, Cvar_FindVar(var_name));
}

static int ltrem_cvar_set (lua_State *L) {
  const char *var_name = lua_tostring(L, 1);
  const char *value = lua_tostring(L, 2);
  if (!var_name || !value) {
    lua_pushnil(L);
    return 1;
  }
  Cvar_SetSafe(var_name, value);
  return ltrem_cvar(L, Cvar_FindVar(var_name));
}

static const luaL_Reg tremlib[] = {
  {"cmd_exec", ltrem_cmd_exec},
  {"cvar_get", ltrem_cvar_get},
  {"cvar_set", ltrem_cvar_set},
  {NULL, NULL}
};

LUALIB_API int luaopen_trem (lua_State *L) {
  luaL_newlib(L, tremlib);
  return 1;
}

