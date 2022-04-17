#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

#include <SDL.h>
#include <SDL_main.h>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <cereal/archives/portable_binary.hpp>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <imgui_stdlib.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
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
using Bytes = std::vector<char>;
extern std::atomic<int32_t> runningThreads;
enum PollState
{
	Error,
	GotData,
	NoData
};

extern bool openSocket(int &, const char *hostname, const char *port, const bool isServer);

extern bool sendData(int, const void *, size_t);

extern PollState pollSocket(const int fd, const int timeout = 1);

extern bool appDone;

extern int runGui();

extern int DefaultPort;

extern size_t MaxPacketSize;

} // namespace TemStream

#include "TemStreamConfig.h"

#include "colors.hpp"

#include "addrinfo.hpp"
#include "memoryStream.hpp"
#include "peerInformation.hpp"

#include "message.hpp"
#include "socket.hpp"

#include "address.hpp"

#include "peer.hpp"
#include "producer.hpp"

#include "clientPeer.hpp"
#include "serverPeer.hpp"

#include "gui.hpp"
#include "query.hpp"
#include "streamDisplay.hpp"
