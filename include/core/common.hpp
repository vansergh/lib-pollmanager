// Sockets //////////////////

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN // Do not include old winsock v1
#include <winsock2.h>
#include <ws2tcpip.h>
#include <cstdint>
#else
#include <sys/socket.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#endif

#ifdef _WIN32
#define VSOCK_INVALID_SOCKET INVALID_SOCKET
#define VSOCK_SOCKET_ERROR SOCKET_ERROR
using SocketID = SOCKET;
#else
#define VSOCK_INVALID_SOCKET -1
#define VSOCK_SOCKET_ERROR -1
using SocketID = int;
#endif

// PollManager //////////////////

#ifdef _WIN32
#include <pollmanager/wepoll/wepoll.h>
using EpollID = HANDLE;
#define VSOCK_EPOLL_ERROR NULL
#define VSOCK_REBIND_OPTION SO_REUSEADDR
#else
#include <sys/epoll.h>
#include <sys/eventfd.h>
using EpollID = int;
#define VSOCK_EPOLL_ERROR -1
#define VSOCK_REBIND_OPTION SO_REUSEPORT
#endif

#ifndef EPOLLABORT
#define EPOLLABORT (1U << 21)
#endif

#define VSOCK_EPOLL_TIMEOUT -1
#define VSOCK_EPOLL_MAX_EVENTS 5

