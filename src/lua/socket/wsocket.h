#ifndef WSOCKET_H
#define WSOCKET_H
/*=========================================================================*\
* Socket compatibilization module for Win32
* LuaSocket toolkit
\*=========================================================================*/

/*=========================================================================*\
* WinSock include files
\*=========================================================================*/
#include <winsock2.h>
#include <ws2tcpip.h>

typedef int socklen_t;
typedef SOCKET t_socket;
typedef t_socket *p_socket;

#define SOCKET_INVALID (INVALID_SOCKET)

#ifndef SO_REUSEPORT
#define SO_REUSEPORT SO_REUSEADDR
#endif

const char *inet_ntop(int af, const void *src, char *dst, socklen_t cnt);

#endif /* WSOCKET_H */
