#ifndef MINIFTPD_SOCKET_API_H
#define MINIFTPD_SOCKET_API_H

#include <exec/types.h>
#include <exec/libraries.h>

#define MINIFTPD_AF_INET 2
#define MINIFTPD_SOCK_STREAM 1
#define MINIFTPD_IPPROTO_TCP 6
#define MINIFTPD_INADDR_ANY 0UL
#define MINIFTPD_FIONBIO 0x8004667eUL
#define MINIFTPD_EINTR 4
#define MINIFTPD_EAGAIN_OLD 11
#define MINIFTPD_EWOULDBLOCK 35

struct MiniFtpdFdSet
{
    ULONG bits;
};

struct MiniFtpdTimeVal
{
    LONG tv_sec;
    LONG tv_usec;
};

struct MiniFtpdInAddr
{
    ULONG s_addr;
};

struct MiniFtpdSockAddr
{
    UBYTE sa_len;
    UBYTE sa_family;
    char sa_data[14];
};

struct MiniFtpdSockAddrIn
{
    UBYTE sin_len;
    UBYTE sin_family;
    UWORD sin_port;
    struct MiniFtpdInAddr sin_addr;
    char sin_zero[8];
};

#define MINIFTPD_FD_ZERO(setp) ((setp)->bits = 0UL)
#define MINIFTPD_FD_SET(fd,setp) \
    do { if ((fd) >= 0 && (fd) < 32) (setp)->bits |= (1UL << (fd)); } while (0)
#define MINIFTPD_FD_ISSET(fd,setp) \
    ((fd) >= 0 && (fd) < 32 && (((setp)->bits & (1UL << (fd))) != 0))

int miniftpd_socket(struct Library *base, int domain, int type, int protocol);
int miniftpd_bind(struct Library *base, int fd,
                  const struct MiniFtpdSockAddr *addr, int addrlen);
int miniftpd_listen(struct Library *base, int fd, int backlog);
int miniftpd_accept(struct Library *base, int fd,
                    struct MiniFtpdSockAddr *addr, int *addrlen);
int miniftpd_send(struct Library *base, int fd,
                  const void *buffer, int length, int flags);
int miniftpd_recv(struct Library *base, int fd,
                  void *buffer, int length, int flags);
int miniftpd_ioctl(struct Library *base, int fd, ULONG request, void *argument);
int miniftpd_close_socket(struct Library *base, int fd);
int miniftpd_wait_select(struct Library *base, int nfds,
                         struct MiniFtpdFdSet *readfds,
                         struct MiniFtpdFdSet *writefds,
                         struct MiniFtpdFdSet *exceptfds,
                         const struct MiniFtpdTimeVal *timeout,
                         ULONG *signals);
int miniftpd_socket_errno(struct Library *base);

#endif
