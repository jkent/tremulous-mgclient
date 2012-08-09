#include "cl_lua.h"

qboolean cl_luaPrintf = qfalse;

#define CL_LUA_WRITEBUF_SIZE 4096
static char cl_luaWriteBuf[CL_LUA_WRITEBUF_SIZE];
static char *cl_luaWritePtr = cl_luaWriteBuf;


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
