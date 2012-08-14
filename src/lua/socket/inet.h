#ifndef INET_H 
#define INET_H 
/*=========================================================================*\
* Internet domain functions
* LuaSocket toolkit
*
* This module implements the creation and connection of internet domain
* sockets, on top of the socket.h interface, and the interface of with the
* resolver. 
*
* The function inet_aton is provided for the platforms where it is not
* available. The module also implements the interface of the internet
* getpeername and getsockname functions as seen by Lua programs.
*
* The Lua functions toip and tohostname are also implemented here.
\*=========================================================================*/
#include "../lua.h"
#include "socket.h"
#include "timeout.h"

#ifdef _WIN32
#define INET_ATON
#endif

int inet_open(lua_State *L);

const char *inet_trycreate(p_socket ps, int domain, int type);
const char *inet_tryconnect(p_socket ps, const char *address,
        const char *serv, p_timeout tm, struct addrinfo *connecthints);
const char *inet_trybind(p_socket ps, const char *address, const char *serv,
        struct addrinfo *bindhints);

int inet_meth_getpeername(lua_State *L, p_socket ps);
int inet_meth_getsockname(lua_State *L, p_socket ps);

#ifdef INET_ATON
int inet_aton(const char *cp, struct in_addr *inp);
#endif

#endif /* INET_H */
