#ifndef _CL_SCRIPT_H
#define _CL_SCRIPT_H

#include "../qcommon/q_shared.h"
#include "../qcommon/qcommon.h"
#include "../lua/lua.h"
#include "../lua/lauxlib.h"
#include "../lua/lualib.h"

#define MAX_LUAFILE 16384

extern qboolean cl_luaCmdExec;

void CL_LuaInit(void);
void CL_LuaShutdown(void);
void CL_LuaRestart(void);
qboolean CL_LuaLoadFile(const char *filename, int rets);
void CL_LuaConsoleHook(const char *text);
qboolean CL_LuaCommandHook(void);

#endif

