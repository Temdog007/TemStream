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
    shutdown(sockfd, SHUT_RD);
    close(sockfd);
#endif
}

int
openSocket(void* data, const SocketOptions options)
{
    int fd = INVALID_SOCKET;
    struct addrinfo* res = (struct addrinfo*)data;
    struct sockaddr* addr = (struct sockaddr*)data;

    const bool isTcp = (options & SocketOptions_Tcp) != 0;
    const bool isLocal = (options & SocketOptions_Local) != 0;

#if _DEBUG
    char buffer[64] = { 0 };
    int port;
    getAddrInfoString(res, buffer, &port);
    printf("Attempting to open '%s' socket: %s:%d\n",
           isTcp ? "tcp" : "udp",
           buffer,
           port);
#endif

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
        if (isTcp && listen(fd, 10) == -1) {
            perror("listen");
            closeSocket(fd);
            fd = INVALID_SOCKET;
            goto end;
        }
    }

#if _DEBUG
    puts("Opened socket");
#endif

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
sendPrepareMessage(const int sockfd, const uint64_t size)
{
    Message message = { 0 };
    message.tag = MessageTag_prepareForData;
    message.prepareForData = size;
    uint8_t buffer[16] = { 0 };
    Bytes bytes = {
        .allocator = NULL, .buffer = buffer, .size = sizeof(buffer), .used = 0
    };
    MESSAGE_SERIALIZE(message, bytes);
    return socketSend(sockfd, &bytes, false);
}

bool
clientSend(const Client* client, const Bytes* bytes, const bool sendSize)
{
    if (sendSize) {
        if (!sendPrepareMessage(client->sockfd, bytes->used)) {
            return false;
        }
    }
    return socketSend(client->sockfd, bytes, false);
}

bool
socketSend(const int sockfd, const Bytes* bytes, const bool exitOnError)
{
#if _DEBUG
    printf("Sending %u bytes to peer...\n", bytes->used);
#endif
    const ssize_t target = (ssize_t)bytes->used;
    struct pollfd pfds = { .events = POLLOUT, .revents = 0, .fd = sockfd };
    ssize_t sent = 0;
    while (sent < target) {
        switch (poll(&pfds, 1, LONG_POLL_WAIT)) {
            case -1:
                perror("poll");
                return false;
            case 0:
                printf("Packet send timeout occurred. Only sent %u bytes\n",
                       bytes->used);
                return false;
            default:
                break;
        }
        const ssize_t current =
          send(sockfd, bytes->buffer + sent, target - sent, 0);
        if (current <= 0) {
            perror("send");
            if (exitOnError) {
                appDone = true;
            }
            return false;
        }
#if _DEBUG
        printf("Sent %zd bytes\n", current);
#endif
        sent += current;
    }
#if _DEBUG
    printf("Sent all bytes\n");
#endif
    SDL_Delay(500);
    return true;
}

bool
readAllData(const int sockfd,
            const uint64_t totalSize,
            pMessage message,
            pBytes bytes)
{
    bool result = false;
#if _DEBUG
    printf("Waiting for %" PRIu64 " bytes from peer...\n", totalSize);
#endif
    bytes->used = 0;
    uint8_t buffer[KB(10)];
    struct pollfd pfds = { .fd = sockfd, .events = POLLIN, .revents = 0 };
    while (bytes->used < totalSize) {
        switch (poll(&pfds, 1, LONG_POLL_WAIT)) {
            case -1:
                perror("poll");
                goto end;
            case 0:
                printf("Peer timed-out when waiting for %" PRIu64
                       " bytes. Only got %u bytes\n",
                       totalSize,
                       bytes->used);
                goto end;
            default:
                break;
        }
        if ((pfds.revents & POLLIN) == 0) {
            continue;
        }
        const ssize_t size = recv(sockfd, buffer, sizeof(buffer), 0);
        if (size < 0) {
            perror("recv");
            goto end;
        }
        if (size == 0) {
            // Handle bytes acquired
            break;
        }
#if _DEBUG
        printf("Received %zd bytes\n", size);
#endif
        for (ssize_t i = 0; i < size; ++i) {
            uint8_tListAppend(bytes, &buffer[i]);
        }
    }
#if _DEBUG
    printf("Got %u bytes from peer\n", bytes->used);
#endif
    result = true;
end:
    MESSAGE_DESERIALIZE((*message), (*bytes));
    return result;
}