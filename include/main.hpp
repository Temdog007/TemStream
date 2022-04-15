#pragma once

#include <array>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <variant>
#include <vector>

#include <SDL.h>

#include <cereal/archives/portable_binary.hpp>
#include <cereal/cereal.hpp>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define KB(X) (X * 1024)
#define MB(X) (KB(X) * 1024)
#define GB(X) (MB(X) * 1024)

namespace TemStream
{
using Bytes = std::vector<uint8_t>;
enum PollState
{
	Error,
	GotData,
	NoData
};

extern bool openSocket(int &, const char *hostname, const char *port, const bool isServer);

extern bool sendData(int, const uint8_t *, size_t);

extern PollState pollSocket(struct pollfd &con, const int timeout = 1);

extern void *get_in_addr(struct sockaddr *sa);

extern bool appDone;

extern int runServer(int, char **);

extern int runGui();
} // namespace TemStream

#include "TemStreamConfig.h"

#include "addrinfo.hpp"
#include "message.hpp"
#include "peer.hpp"
#include "producer.hpp"