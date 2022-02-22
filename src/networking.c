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

#if _DEBUG
    char buffer[64] = { 0 };
    int port;
    getAddrInfoString(res, buffer, &port);
    printf("Attempting to open socket: %s:%d\n", buffer, port);
#endif

    const bool isTcp = (options & SocketOptions_Tcp) != 0;
    const bool isLocal = (options & SocketOptions_Local) != 0;
    if (isLocal) {
        fd = socket(AF_UNIX, isTcp ? SOCK_STREAM : SOCK_DGRAM, 0);
    } else {
        fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    }
    if (fd <= 0) {
        perror("socket");
        goto end;
    }

    if (!isLocal) {
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            closeSocket(fd);
            fd = INVALID_SOCKET;
            goto end;
        }
        if (isTcp) {
            int yes = 1;
            if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) ==
                -1) {
                perror("setsockopt");
                closeSocket(fd);
                fd = INVALID_SOCKET;
                goto end;
            }
        }
    }

    if ((options & SocketOptions_Server) == 0) {
        if ((isLocal ? connect(fd, addr, sizeof(struct sockaddr_un))
                     : connect(fd, res->ai_addr, res->ai_addrlen)) == -1) {
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
        if (isTcp && listen(fd, 128) == -1) {
            perror("listen");
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
    hints.ai_family = AF_INET;
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
    const bool isServer = (options & SocketOptions_Server) != 0;
    struct sockaddr_un addr = { 0 };
    addr.sun_family = AF_UNIX;
    snprintf(addr.sun_path,
             sizeof(addr.sun_path),
             "%s_%s",
             filename,
             (options & SocketOptions_Tcp) == 0 ? "udp" : "tcp");
    if (isServer) {
        unlink(addr.sun_path);
    }
    const int fd =
      openSocket((struct sockaddr*)&addr, options | SocketOptions_Local);
    if (isServer && fd <= 0) {
        unlink(addr.sun_path);
    }
    return fd;
}

const char*
getAddrString(const struct sockaddr_storage* addr, char ipstr[64], int* port)
{
    switch (addr->ss_family) {
        case AF_INET: {
            struct sockaddr_in* s = (struct sockaddr_in*)addr;
            if (port != NULL) {
                *port = ntohs(s->sin_port);
            }
            return inet_ntop(AF_INET, &s->sin_addr, ipstr, 64);
        } break;
        case AF_INET6: {
            struct sockaddr_in6* s = (struct sockaddr_in6*)addr;
            if (port != NULL) {
                *port = ntohs(s->sin6_port);
            }
            return inet_ntop(AF_INET6, &s->sin6_addr, ipstr, 64);
        } break;
        case AF_UNIX: {
            struct sockaddr_un* s = (struct sockaddr_un*)addr;
            snprintf(ipstr, 64, "%s", s->sun_path);
            if (port != NULL) {
                *port = 0;
            }
            return ipstr;
        } break;
        default:
            if (port != NULL) {
                *port = 0;
            }
            return ipstr;
    }
}

const char*
getAddrInfoString(const struct addrinfo* addr, char ipstr[64], int* port)
{
    switch (addr->ai_family) {
        case AF_INET: {
            struct sockaddr_in* s = (struct sockaddr_in*)addr->ai_addr;
            if (port != NULL) {
                *port = ntohs(s->sin_port);
            }
            return inet_ntop(AF_INET, &s->sin_addr, ipstr, 64);
        } break;
        case AF_INET6: {
            struct sockaddr_in6* s = (struct sockaddr_in6*)addr->ai_addr;
            if (port != NULL) {
                *port = ntohs(s->sin6_port);
            }
            return inet_ntop(AF_INET6, &s->sin6_addr, ipstr, 64);
        } break;
        case AF_UNIX: {
            struct sockaddr_un* s = (struct sockaddr_un*)addr->ai_addr;
            snprintf(ipstr, 64, "%s", s->sun_path);
            if (port != NULL) {
                *port = 0;
            }
            return ipstr;
        } break;
        default:
            if (port != NULL) {
                *port = 0;
            }
            return ipstr;
    }
}

int
openSocketFromAddress(const Address* address, const SocketOptions options)
{
    switch (address->tag) {
        case AddressTag_domainSocket:
            return openUnixSocket(address->domainSocket.buffer, options);
        case AddressTag_ipAddress:
            return openIpSocket(address->ipAddress.ip.buffer,
                                address->ipAddress.port.buffer,
                                options);
        default:
            break;
    }
    return INVALID_SOCKET;
}

bool
clientSend(const Client* client, const Bytes* bytes)
{
    return socketSend(client->sockfd, bytes, false);
}

bool
socketSend(const int sockfd, const Bytes* bytes, const bool exitOnError)
{
    if (send(sockfd, bytes->buffer, bytes->used, 0) != (ssize_t)bytes->used) {
        perror("send");
        if (exitOnError) {
            appDone = true;
        }
        return false;
    }
    return true;
}