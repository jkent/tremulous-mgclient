/*=========================================================================*\
* Internet domain functions
* LuaSocket toolkit
*
* RCS ID: $Id: inet.c,v 1.28 2005/10/07 04:40:59 diego Exp $
\*=========================================================================*/
#include <stdio.h>
#include <string.h>

#include "../lua.h"
#include "../lauxlib.h"

#include "inet.h"

/*=========================================================================*\
* Internal function prototypes.
\*=========================================================================*/
static int inet_global_toip(lua_State *L);
static int inet_global_getaddrinfo(lua_State *L);
static int inet_global_tohostname(lua_State *L);
static void inet_pushresolved(lua_State *L, struct hostent *hp);
static int inet_global_gethostname(lua_State *L);

/* DNS functions */
static luaL_Reg func[] = {
    { "toip", inet_global_toip},
    { "getaddrinfo", inet_global_getaddrinfo},
    { "tohostname", inet_global_tohostname},
    { "gethostname", inet_global_gethostname},
    { NULL, NULL}
};

/*=========================================================================*\
* Exported functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Initializes module
\*-------------------------------------------------------------------------*/
int inet_open(lua_State *L)
{
    lua_pushstring(L, "dns");
    lua_newtable(L);
    luaL_openlib(L, NULL, func, 0);
    lua_settable(L, -3);
    return 0;
}

/*=========================================================================*\
* Global Lua functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Returns all information provided by the resolver given a host name
* or ip address
\*-------------------------------------------------------------------------*/
static int inet_gethost(const char *address, struct hostent **hp) {
    struct in_addr addr;
    if (inet_aton(address, &addr))
        return socket_gethostbyaddr((char *) &addr, sizeof(addr), hp);
    else
        return socket_gethostbyname(address, hp);
}

/*-------------------------------------------------------------------------*\
* Returns all information provided by the resolver given a host name
* or ip address
\*-------------------------------------------------------------------------*/
static int inet_global_tohostname(lua_State *L) {
    const char *address = luaL_checkstring(L, 1);
    struct hostent *hp = NULL;
    int err = inet_gethost(address, &hp);
    if (err != IO_DONE) {
        lua_pushnil(L);
        lua_pushstring(L, socket_hoststrerror(err));
        return 2;
    }
    lua_pushstring(L, hp->h_name);
    inet_pushresolved(L, hp);
    return 2;
}

/*-------------------------------------------------------------------------*\
* Returns all information provided by the resolver given a host name
* or ip address
\*-------------------------------------------------------------------------*/
static int inet_global_toip(lua_State *L)
{
    const char *address = luaL_checkstring(L, 1);
    struct hostent *hp = NULL;
    int err = inet_gethost(address, &hp);
    if (err != IO_DONE) {
        lua_pushnil(L);
        lua_pushstring(L, socket_hoststrerror(err));
        return 2;
    }
    lua_pushstring(L, inet_ntoa(*((struct in_addr *) hp->h_addr)));
    inet_pushresolved(L, hp);
    return 2;
}

static int inet_global_getaddrinfo(lua_State *L)
{
    const char *hostname = luaL_checkstring(L, 1);
    struct addrinfo *iterator = NULL, *resolved = NULL;
    struct addrinfo hints;
    int i = 1, ret = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = PF_UNSPEC;
    ret = getaddrinfo(hostname, NULL, &hints, &resolved);
    if (ret != 0) {
        lua_pushnil(L);
        lua_pushstring(L, socket_gaistrerror(ret));
        return 2;
    }
    lua_newtable(L);
    for (iterator = resolved; iterator; iterator = iterator->ai_next) {
        char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
        getnameinfo(iterator->ai_addr, iterator->ai_addrlen, hbuf, sizeof(hbuf),
                sbuf, 0, NI_NUMERICHOST);
        lua_pushnumber(L, i);
        lua_newtable(L);
        switch (iterator->ai_family) {
            case AF_INET:
                lua_pushliteral(L, "family");
                lua_pushliteral(L, "inet");
                lua_settable(L, -3);
                break;
            case AF_INET6:
                lua_pushliteral(L, "family");
                lua_pushliteral(L, "inet6");
                lua_settable(L, -3);
                break;;
        }
        lua_pushliteral(L, "addr");
        lua_pushstring(L, hbuf);
        lua_settable(L, -3);
        lua_settable(L, -3);
        i++;
    }
    freeaddrinfo(resolved);
    return 1;
}


/*-------------------------------------------------------------------------*\
* Gets the host name
\*-------------------------------------------------------------------------*/
static int inet_global_gethostname(lua_State *L)
{
    char name[257];
    name[256] = '\0';
    if (gethostname(name, 256) < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "gethostname failed");
        return 2;
    } else {
        lua_pushstring(L, name);
        return 1;
    }
}



/*=========================================================================*\
* Lua methods
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Retrieves socket peer name
\*-------------------------------------------------------------------------*/
int inet_meth_getpeername(lua_State *L, p_socket ps)
{
    union {
	struct sockaddr_storage sas;
	struct sockaddr sa;
	struct sockaddr_in sa4;
	struct sockaddr_in6 sa6;
    } peer;
    socklen_t peer_len = sizeof(peer);

    if (getpeername(*ps, &peer.sa, &peer_len) < 0) {
        lua_pushnil(L);
        lua_pushfstring(L, "getpeername failed (%d): %s", errno,
                strerror(errno));
    } else {
        char ipaddr[INET6_ADDRSTRLEN] = "";
        unsigned short port = 0;

        switch (peer.sa.sa_family) {
        case AF_INET:
            inet_ntop(AF_INET, &peer.sa4.sin_addr, ipaddr, sizeof(ipaddr));
            port = ntohs(peer.sa4.sin_port);
            break;
        case AF_INET6:
            inet_ntop(AF_INET6, &peer.sa6.sin6_addr, ipaddr, sizeof(ipaddr));
            port = ntohs(peer.sa6.sin6_port);
            break;
        default:
            lua_pushnil(L);
            lua_pushfstring(L, "Unknown address family %d", peer.sa.sa_family);
            return 2;
            break;
        }

        lua_pushstring(L, ipaddr);
        lua_pushnumber(L, port);
    }
    return 2;
}

/*-------------------------------------------------------------------------*\
* Retrieves socket local name
\*-------------------------------------------------------------------------*/
int inet_meth_getsockname(lua_State *L, p_socket ps)
{
    struct sockaddr_in local;
    socklen_t local_len = sizeof(local);
    if (getsockname(*ps, (SA *) &local, &local_len) < 0) {
        lua_pushnil(L);
        lua_pushstring(L, "getsockname failed");
    } else {
        lua_pushstring(L, inet_ntoa(local.sin_addr));
        lua_pushnumber(L, ntohs(local.sin_port));
    }
    return 2;
}

/*=========================================================================*\
* Internal functions
\*=========================================================================*/
/*-------------------------------------------------------------------------*\
* Passes all resolver information to Lua as a table
\*-------------------------------------------------------------------------*/
static void inet_pushresolved(lua_State *L, struct hostent *hp)
{
    char **alias;
    struct in_addr **addr;
    int i, resolved;
    lua_newtable(L); resolved = lua_gettop(L);
    lua_pushstring(L, "name");
    lua_pushstring(L, hp->h_name);
    lua_settable(L, resolved);
    lua_pushstring(L, "ip");
    lua_pushstring(L, "alias");
    i = 1;
    alias = hp->h_aliases;
    lua_newtable(L);
    if (alias) {
        while (*alias) {
            lua_pushnumber(L, i);
            lua_pushstring(L, *alias);
            lua_settable(L, -3);
            i++; alias++;
        }
    }
    lua_settable(L, resolved);
    i = 1;
    lua_newtable(L);
    addr = (struct in_addr **) hp->h_addr_list;
    if (addr) {
        while (*addr) {
            lua_pushnumber(L, i);
            lua_pushstring(L, inet_ntoa(**addr));
            lua_settable(L, -3);
            i++; addr++;
        }
    }
    lua_settable(L, resolved);
}

/*-------------------------------------------------------------------------*\
* Tries to create a new inet socket
\*-------------------------------------------------------------------------*/
const char *inet_trycreate(p_socket ps, int domain, int type) {
    return socket_strerror(socket_create(ps, domain, type, 0));
}

/*-------------------------------------------------------------------------*\
* Tries to connect to remote address (address, port)
\*-------------------------------------------------------------------------*/
const char *inet_tryconnect(p_socket ps, const char *address,
        const char *serv, p_timeout tm, struct addrinfo *connecthints)
{
    struct addrinfo *iterator = NULL, *resolved = NULL;
    const char *err = NULL;
    /* try resolving */
    err = socket_gaistrerror(getaddrinfo(address, serv,
                connecthints, &resolved));
    if (err != NULL) {
        if (resolved) freeaddrinfo(resolved);
        return err;
    }
    /* iterate over all returned addresses trying to connect */
    for (iterator = resolved; iterator; iterator = iterator->ai_next) {
        timeout_markstart(tm);
        /* try connecting to remote address */
        err = socket_strerror(socket_connect(ps,
            (SA *) iterator->ai_addr,
            iterator->ai_addrlen, tm));
        /* if success, break out of loop */
        if (err == NULL) break;
    }

    freeaddrinfo(resolved);
    /* here, if err is set, we failed */
    return err;
}

/*-------------------------------------------------------------------------*\
* Tries to bind socket to (address, port)
\*-------------------------------------------------------------------------*/
const char *inet_trybind(p_socket ps, const char *address, const char *serv,
        struct addrinfo *bindhints)
{
    struct addrinfo *iterator = NULL, *resolved = NULL;
    const char *err = NULL;
    /* translate luasocket special values to C */
    if (strcmp(address, "*") == 0) address = NULL;
    if  (!serv) serv = "0";
    /* try resolving */
    err = socket_gaistrerror(getaddrinfo(address, serv,
            bindhints, &resolved));
    if (err) {
        if (resolved) freeaddrinfo(resolved);
        return err;
    }
    /* iterate over resolved addresses until one is good */
    for (iterator = resolved; iterator; iterator = iterator->ai_next) {
        /* try binding to local address */
        err = socket_strerror(socket_bind(ps,
            (SA *) iterator->ai_addr,
            iterator->ai_addrlen));
        /* if faiiled, we try the next one */
        if (err != NULL) socket_destroy(ps);
        /* if success, we abort loop */
        else break;
    }
    /* cleanup and return error */
    freeaddrinfo(resolved);
    return err;
}

/*-------------------------------------------------------------------------*\
* Some systems do not provide this so that we provide our own. It's not
* marvelously fast, but it works just fine.
\*-------------------------------------------------------------------------*/
#ifdef INET_ATON
int inet_aton(const char *cp, struct in_addr *inp)
{
    unsigned int a = 0, b = 0, c = 0, d = 0;
    int n = 0, r;
    unsigned long int addr = 0;
    r = sscanf(cp, "%u.%u.%u.%u%n", &a, &b, &c, &d, &n);
    if (r == 0 || n == 0) return 0;
    cp += n;
    if (*cp) return 0;
    if (a > 255 || b > 255 || c > 255 || d > 255) return 0;
    if (inp) {
        addr += a; addr <<= 8;
        addr += b; addr <<= 8;
        addr += c; addr <<= 8;
        addr += d;
        inp->s_addr = htonl(addr);
    }
    return 1;
}
#endif


