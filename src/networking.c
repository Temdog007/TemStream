#include <include/main.h>

void
#if __EMSCRIPTEN__
  EMSCRIPTEN_KEEPALIVE
#endif
  closeSocket(const int sockfd)
{
#if __EMSCRIPTEN__
    emscripten_websocket_close(sockfd, 0, "Close requested");
    emscripten_websocket_delete(sockfd);
#else
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
#endif
}

int
openSocket(void* data, const SocketOptions options)
{
    int fd = INVALID_SOCKET;
    struct addrinfo* res = (struct addrinfo*)data;
    struct sockaddr* addr = (struct sockaddr*)data;
    const bool isLocal = (options & SocketOptions_Local) != 0;
    if (isLocal) {
        fd =
          socket(AF_UNIX,
                 (options & SocketOptions_Tcp) == 0 ? SOCK_DGRAM : SOCK_STREAM,
                 0);
    } else {
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    }
    if (fd <= 0) {
        perror("socket");
        goto end;
    }

    if ((options & SocketOptions_Server) == 0) {
        if ((isLocal ? connect(fd, addr, sizeof(struct sockaddr_un))
                     : connect(fd, res->ai_addr, res->ai_addrlen)) <= 0) {
            perror("connect");
            closeSocket(fd);
            fd = INVALID_SOCKET;
            goto end;
        }
    } else {
        if ((isLocal ? bind(fd, addr, sizeof(struct sockaddr_un))
                     : bind(fd, res->ai_addr, res->ai_addrlen)) == -1) {
            perror("bind");
            closeSocket(fd);
            fd = INVALID_SOCKET;
            goto end;
        }
    }

    if (!isLocal) {
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            closeSocket(fd);
            fd = INVALID_SOCKET;
            goto end;
        }
    }

end:
    return fd;
}

int
openIpSocket(const char* ip, const char* port, const SocketOptions options)
{
    struct addrinfo hints = { 0 };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype =
      (options & SocketOptions_Tcp) == 0 ? SOCK_DGRAM : SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    int fd = INVALID_SOCKET;
    struct addrinfo* res = NULL;
    const int status = getaddrinfo(ip, port, &hints, &res);
    if (status == 0) {
        fd = openSocket(res, options);
    } else {
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(status));
    }

    if (res != NULL) {
        freeaddrinfo(res);
    }
    return fd;
}

int
openUnixSocket(const char* filename, SocketOptions options)
{

    struct sockaddr_un addr = { 0 };
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path,
             sizeof(addr.sun_path),
             "%s_%s",
             filename,
             (options & SocketOptions_Tcp) == 0 ? "udp" : "tcp");
    unlink(addr.sun_path);
    const int fd = openSocket((struct sockaddr*)&addr, options);
    if (fd <= 0) {
        unlink(addr.sun_path);
    }
    return fd;
}

const char*
getAddrString(struct sockaddr_storage* addr, char ipstr[64])
{
    if (addr->ss_family == AF_INET) {
        struct sockaddr_in* s = (struct sockaddr_in*)&addr;
        return inet_ntop(AF_INET, &s->sin_addr, ipstr, 64);
    } else { // AF_INET6
        struct sockaddr_in6* s = (struct sockaddr_in6*)&addr;
        return inet_ntop(AF_INET6, &s->sin6_addr, ipstr, 64);
    }
}

int
sendTcp(const int fd,
        const void* buf,
        const size_t size,
        const struct sockaddr_storage* addr)
{
    return send(fd, buf, size, 0);
}

int
sendUdp(const int fd,
        const void* buf,
        const size_t size,
        const struct sockaddr_storage* addr)
{
    return sendto(fd, buf, size, 0, (struct sockaddr*)addr, sizeof(*addr));
}