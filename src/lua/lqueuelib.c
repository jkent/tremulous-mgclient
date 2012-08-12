#include "lqueuelib.h"
#include "lauxlib.h"

#ifdef USE_LOCAL_HEADERS
#   include "SDL.h"
#	include "SDL_thread.h"
#else
#   include <SDL.h>
#	include <SDL_thread.h>
#endif

static lua_State *master = NULL;

static lua_State *master_storage = NULL;
static lua_State *slave_storage = NULL;

static SDL_mutex *master_storage_lock = NULL;
static SDL_mutex *slave_storage_lock = NULL;
static SDL_cond *master_storage_cond = NULL;
static SDL_cond *slave_storage_cond = NULL;


/* The code for copying nested tables comes from the lua-llthreads
 * project. [1]
 *
 * [1] https://github.com/Neopallium/lua-llthreads */

#include "lua.h"

#define MAX_COPY_DEPTH  30

struct copy_state {
	lua_State *src;
	lua_State *dst;
	int has_cache;
	int cache_idx;
};


static int
copy_table_from_cache(struct copy_state *state, int idx)
{
	void *ptr;

	/* convert table to pointer for lookup in cache */
	ptr = (void *) lua_topointer(state->src, idx);
	if ( ptr == NULL )
		return 0; /* can't convert pointer */

	/* check if we need to create the cache */
	if ( !state->has_cache ) {
		lua_newtable(state->dst);
		lua_replace(state->dst, state->cache_idx);
		state->has_cache = 1;
	}

	lua_pushlightuserdata(state->dst, ptr);
	lua_rawget(state->dst, state->cache_idx);
	if ( lua_isnil(state->dst, -1) ) {
		/* not in cache */
		lua_pop(state->dst, 1);
		/* create new table and add to cache */
		lua_newtable(state->dst);
		lua_pushlightuserdata(state->dst, ptr);
		lua_pushvalue(state->dst, -2);
		lua_rawset(state->dst, state->cache_idx);
		return 0;
	}

	/* found table in cache */
	return 1;
}


static int
copy_one_value(struct copy_state *state, int depth, int idx)
{
	/* Maximum recursive depth */
	if ( ++depth > MAX_COPY_DEPTH ) {
		return luaL_error(state->src, "Hit maximum copy depth (%d > %d).",
				depth, MAX_COPY_DEPTH);
	}

	/* only support string/number/boolean/nil/table/lightuserdata */
	switch ( lua_type(state->src, idx) ) {
	case LUA_TNUMBER:
		lua_pushnumber(state->dst, lua_tonumber(state->src, idx));
		break;
	case LUA_TBOOLEAN:
		lua_pushboolean(state->dst, lua_toboolean(state->src, idx));
		break;
	case LUA_TSTRING: {
		size_t length;
		const char *string = lua_tolstring(state->src, idx, &length);
		lua_pushlstring(state->dst, string, length);
		break;
	}
	case LUA_TLIGHTUSERDATA: {
		lua_pushlightuserdata(state->dst, lua_touserdata(state->src, idx));
		break;
	}
	case LUA_TNIL:
		lua_pushnil(state->dst);
		break;
	case LUA_TTABLE:
		/* make sure there is room on the new state for 3 values
		 * (table,key,value) */
		if ( !lua_checkstack(state->dst, 3) ) {
			return luaL_error(state->src, "To stack overflow!");
		}
		/* make room on from stack for key/value pairs */
		luaL_checkstack(state->src, 2, "From stack overflow");

		/* check cache for table */
		if ( copy_table_from_cache(state, idx) ) {
			/* found in cache; don't need to copy table */
			break;
		}

		lua_pushnil(state->src);
		while ( lua_next(state->src, idx) != 0 ) {
			/* key is at (top - 1), value at (top), but we need to normalize
			 * these to positive indices */
			int kv_pos = lua_gettop(state->src);
			/* copy key */
			copy_one_value(state, depth, kv_pos - 1);
			/* copy value */
			copy_one_value(state, depth, kv_pos);
			/* Copied key and value are now at -2 and -1 in dest */
			lua_settable(state->dst, -3);
			/* Pop value for next iteration */
			lua_pop(state->src, 1);
		}
		break;

	case LUA_TFUNCTION:
	case LUA_TUSERDATA:
	case LUA_TTHREAD:
	default:
		return luaL_argerror(state->src, idx,
				"function/userdata/thread types unsupported");
		break;
	}

	return 1;
}


/*
** Copies values from State src to State dst.
*/
static void
copy_values(lua_State *dst, lua_State *src, int i)
{
	struct copy_state state;
	int top;

	top = lua_gettop(src);
	luaL_checkstack(dst, top - i + 1, "To stack overflow!");

	/* setup copy state */
	state.src = src;
	state.dst = dst;
	state.has_cache = 0;
	lua_pushnil(dst);
	state.cache_idx = lua_gettop(dst);

	for ( ; i <= top; i++ ) {
		copy_one_value(&state, 0, i);
	}
	/* remove cache table */
	lua_remove(dst, state.cache_idx);
}


static int lqueue_write (lua_State *src)
{
	int len;
	lua_State *dst;
	SDL_mutex *dst_lock;
	SDL_cond *dst_cond;

	if (src == master) {
		dst = master_storage;
		dst_lock = master_storage_lock;
		dst_cond = master_storage_cond;
	}
	else {
		dst = slave_storage;
		dst_lock = slave_storage_lock;
		dst_cond = slave_storage_cond;
	}

	SDL_mutexP(dst_lock);

	luaL_getsubtable(dst, LUA_REGISTRYINDEX, "queue");
	len = luaL_len(dst, -1);
	copy_values(dst, src, 1);
	lua_pop(src, 1);
	lua_rawseti(dst, -2, len+1);
	lua_pop(dst, 1);

	SDL_mutexV(dst_lock);
	SDL_CondSignal(dst_cond);

	return 0;
}


static int lqueue_read (lua_State *dst)
{
	int len, i;
	lua_State *src;
	SDL_mutex *src_lock;
	SDL_cond *src_cond;

	if (dst == master) {
		src = slave_storage;
		src_lock = slave_storage_lock;
		src_cond = slave_storage_cond;
	}
	else {
		src = master_storage;
		src_lock = master_storage_lock;
		src_cond = master_storage_cond;
	}

	SDL_mutexP(src_lock);

	luaL_getsubtable(src, LUA_REGISTRYINDEX, "queue");
	while ((len = luaL_len(src, -1)) == 0) {
		SDL_CondWait(src_cond, src_lock);
	}

	lua_rawgeti(src, -1, 1);
	copy_values(dst, src, 1);
	for (i = 1; i < len; i++) {
		lua_rawgeti(src, -2, i+1);
		lua_rawseti(src, -3, i);
	}
	lua_pushnil(src);
	lua_rawseti(src, -3, len);

	lua_pop(src, 2);

	SDL_mutexV(src_lock);

	return 1;
}


static int lqueue_read_count (lua_State *L)
{
	int len;
	lua_State *src;
	SDL_mutex *src_lock;
	SDL_cond *src_cond;

	if (L == master) {
		src = slave_storage;
		src_lock = slave_storage_lock;
		src_cond = slave_storage_cond;
	}
	else {
		src = master_storage;
		src_lock = master_storage_lock;
		src_cond = master_storage_cond;
	}

	SDL_mutexP(src_lock);

	luaL_getsubtable(src, LUA_REGISTRYINDEX, "queue");
	len = luaL_len(src, -1);
	lua_pop(src, 1);
	lua_pushinteger(L, len);

	SDL_mutexV(src_lock);

	return 1;
}


static int lqueue_readable (lua_State *L)
{
	int len;
	lua_State *src;
	SDL_mutex *src_lock;
	SDL_cond *src_cond;

	if (L == master) {
		src = slave_storage;
		src_lock = slave_storage_lock;
		src_cond = slave_storage_cond;
	}
	else {
		src = master_storage;
		src_lock = master_storage_lock;
		src_cond = master_storage_cond;
	}

	SDL_mutexP(src_lock);

	luaL_getsubtable(src, LUA_REGISTRYINDEX, "queue");
	len = luaL_len(src, -1);
	lua_pop(src, 1);
	lua_pushboolean(L, (len != 0));

	SDL_mutexV(src_lock);

	return 1;
}


static void lqueue_cleanup(void)
{
	if (master_storage) {
		lua_close(master_storage);
		master_storage = NULL;
	}
	if (master_storage_lock) {
		SDL_DestroyMutex(master_storage_lock);
		master_storage_lock = NULL;
	}
	if (master_storage_cond) {
		SDL_DestroyCond(master_storage_cond);
		master_storage_cond = NULL;
	}
	if (slave_storage) {
		lua_close(slave_storage);
		slave_storage = NULL;
	}
	if (slave_storage_lock) {
		SDL_DestroyMutex(slave_storage_lock);
		slave_storage_lock = NULL;
	}
	if (slave_storage_cond) {
		SDL_DestroyCond(slave_storage_cond);
		slave_storage_cond = NULL;
	}
}


static const luaL_Reg queuelib[] = {
	{"write", lqueue_write},
	{"read", lqueue_read},
	{"read_count", lqueue_read_count},
	{"readable", lqueue_readable},
	{NULL, NULL}
};


LUALIB_API int luaopen_queue (lua_State *L)
{
	if (!master) {
		master = L;

		master_storage = luaL_newstate();
		master_storage_lock = SDL_CreateMutex();
		master_storage_cond = SDL_CreateCond();

		slave_storage = luaL_newstate();
		slave_storage_lock = SDL_CreateMutex();
		slave_storage_cond = SDL_CreateCond();

		if (!master_storage || !master_storage_lock || !master_storage_cond ||
			!slave_storage  || !slave_storage_lock  || !slave_storage_cond) {
			lqueue_cleanup();
			/* TODO: convey this as an error message somehow */
			return 0;
		}
	}

	luaL_newlib(L, queuelib);
	return 1;
}


LUALIB_API void luaclose_queue (lua_State *L)
{
	if (L == master) {
		lqueue_cleanup();
		master = NULL;
	}
}
