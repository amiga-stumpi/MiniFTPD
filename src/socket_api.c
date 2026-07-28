#include "miniftpd/socket_api.h"

int miniftpd_socket(struct Library *base, int domain, int type, int protocol)
{
    register int d0 __asm("d0") = domain;
    register int d1 __asm("d1") = type;
    register int d2 __asm("d2") = protocol;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-30:W)" : "+r"(d0), "+r"(d1), "+r"(d2)
                    : "r"(a6) : "a0", "a1", "cc", "memory");
    return d0;
}

int miniftpd_bind(struct Library *base, int fd,
                  const struct MiniFtpdSockAddr *addr, int addrlen)
{
    register int d0 __asm("d0") = fd;
    register const struct MiniFtpdSockAddr *a0 __asm("a0") = addr;
    register int d1 __asm("d1") = addrlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-36:W)" : "+r"(d0), "+r"(a0), "+r"(d1)
                    : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

int miniftpd_listen(struct Library *base, int fd, int backlog)
{
    register int d0 __asm("d0") = fd;
    register int d1 __asm("d1") = backlog;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-42:W)" : "+r"(d0), "+r"(d1)
                    : "r"(a6) : "a0", "a1", "cc", "memory");
    return d0;
}

int miniftpd_accept(struct Library *base, int fd,
                    struct MiniFtpdSockAddr *addr, int *addrlen)
{
    register int d0 __asm("d0") = fd;
    register struct MiniFtpdSockAddr *a0 __asm("a0") = addr;
    register int *a1 __asm("a1") = addrlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-48:W)" : "+r"(d0), "+r"(a0), "+r"(a1)
                    : "r"(a6) : "d1", "cc", "memory");
    return d0;
}

int miniftpd_send(struct Library *base, int fd,
                  const void *buffer, int length, int flags)
{
    register int d0 __asm("d0") = fd;
    register const void *a0 __asm("a0") = buffer;
    register int d1 __asm("d1") = length;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-66:W)"
                    : "+r"(d0), "+r"(a0), "+r"(d1), "+r"(d2)
                    : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

int miniftpd_recv(struct Library *base, int fd,
                  void *buffer, int length, int flags)
{
    register int d0 __asm("d0") = fd;
    register void *a0 __asm("a0") = buffer;
    register int d1 __asm("d1") = length;
    register int d2 __asm("d2") = flags;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-78:W)"
                    : "+r"(d0), "+r"(a0), "+r"(d1), "+r"(d2)
                    : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

int miniftpd_getsockname(struct Library *base, int fd,
                          struct MiniFtpdSockAddr *addr, int *addrlen)
{
    register int d0 __asm("d0") = fd;
    register struct MiniFtpdSockAddr *a0 __asm("a0") = addr;
    register int *a1 __asm("a1") = addrlen;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-102:W)" : "+r"(d0), "+r"(a0), "+r"(a1)
                    : "r"(a6) : "d1", "cc", "memory");
    return d0;
}

int miniftpd_ioctl(struct Library *base, int fd, ULONG request, void *argument)
{
    register int d0 __asm("d0") = fd;
    register ULONG d1 __asm("d1") = request;
    register void *a0 __asm("a0") = argument;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-114:W)" : "+r"(d0), "+r"(d1), "+r"(a0)
                    : "r"(a6) : "a1", "cc", "memory");
    return d0;
}

int miniftpd_close_socket(struct Library *base, int fd)
{
    register int d0 __asm("d0") = fd;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-120:W)" : "+r"(d0)
                    : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}

int miniftpd_wait_select(struct Library *base, int nfds,
                         struct MiniFtpdFdSet *readfds,
                         struct MiniFtpdFdSet *writefds,
                         struct MiniFtpdFdSet *exceptfds,
                         const struct MiniFtpdTimeVal *timeout,
                         ULONG *signals)
{
    register int d0 __asm("d0") = nfds;
    register ULONG *d1 __asm("d1") = signals;
    register struct MiniFtpdFdSet *a0 __asm("a0") = readfds;
    register struct MiniFtpdFdSet *a1 __asm("a1") = writefds;
    register struct MiniFtpdFdSet *a2 __asm("a2") = exceptfds;
    register const struct MiniFtpdTimeVal *a3 __asm("a3") = timeout;
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-126:W)"
                    : "+r"(d0), "+r"(d1), "+r"(a0), "+r"(a1),
                      "+r"(a2), "+r"(a3)
                    : "r"(a6) : "cc", "memory");
    return d0;
}

int miniftpd_socket_errno(struct Library *base)
{
    register int d0 __asm("d0");
    register struct Library *a6 __asm("a6") = base;
    __asm volatile ("jsr a6@(-162:W)" : "=r"(d0)
                    : "r"(a6) : "d1", "a0", "a1", "cc", "memory");
    return d0;
}
