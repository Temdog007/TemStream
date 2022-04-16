#pragma once

#include <array>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
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
enum PollState
{
	Error,
	GotData,
	NoData
};

extern bool openSocket(int &, const char *hostname, const char *port, const bool isServer);

extern bool sendData(int, const uint8_t *, size_t);

extern PollState pollSocket(const int fd, const int timeout = 1);

extern void *get_in_addr(struct sockaddr *sa);

extern bool appDone;

extern int runGui();

extern int DefaultPort;

template <class T> bool sendMessage(const T &t, std::mutex &mutex, const int fd)
{
	std::istringstream is;
	cereal::PortableBinaryInputArchive in(is);
	in(t);
	const std::string str(is.str());
	std::lock_guard<std::mutex> guard(mutex);
	return sendData(fd, reinterpret_cast<const uint8_t *>(str.data()), str.size());
}
} // namespace TemStream

#include "TemStreamConfig.h"

#include "addrinfo.hpp"
#include "peerInformation.hpp"

#include "message.hpp"

#include "peer.hpp"
#include "producer.hpp"

#include "clientPeer.hpp"
#include "serverPeer.hpp"

#include "streamDisplay.hpp"