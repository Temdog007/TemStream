#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <codecvt>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

#include <SDL.h>
#include <SDL_main.h>

#include <SDL_image.h>

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <cereal/archives/portable_binary.hpp>

#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <imgui_stdlib.h>

#include <opus.h>

#include <arpa/inet.h>
#include <math.h>
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

#define _DEBUG !NDEBUG
#define USE_CUSTOM_ALLOCATOR true

#define THREADS_AVAILABLE (!__EMSCRIPTEN__ || !SDL_THREADS_DISABLED)

namespace TemStream
{
enum TemStreamEvent : int32_t
{
	ReloadFont = 0xabcd,
	SendSingleMessagePacket,
	SendMessagePackets,
	HandleMessagePacket,
	HandleMessagePackets,
	SetQueryData,
	SetSurfaceToStreamDisplay,
	AddAudio
};
class SDL_MutexWrapper
{
  private:
	SDL_mutex *mutex;

  public:
	SDL_MutexWrapper();
	SDL_MutexWrapper(const SDL_MutexWrapper &) = delete;
	SDL_MutexWrapper(SDL_MutexWrapper &&) = delete;
	~SDL_MutexWrapper();

	void lock();
	void unlock();
};
using Mutex = std::recursive_mutex;

#define LOG_LOCK(M) LogMutex guard(M, #M)
#define DEBUG_MUTEX false
#if DEBUG_MUTEX
#define LOCK(M) LOG_LOCK(M)
#else
#define LOCK(M) std::lock_guard<Mutex> guard(M)
#endif
extern std::atomic<int32_t> runningThreads;
enum PollState
{
	Error,
	GotData,
	NoData
};

template <typename T> inline void hash_combine(std::size_t &seed, const T &v)
{
	std::hash<T> hasher;
	seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

extern bool openSocket(int &, const char *hostname, const char *port, const bool isServer);

extern bool sendData(int, const void *, size_t);

extern PollState pollSocket(const int fd, const int timeout = 1);

extern bool appDone;

extern void initialLogs();

extern bool isTTF(const char *);

extern bool isImage(const char *);

extern void SetWindowMinSize(SDL_Window *window);

extern bool tryPushEvent(SDL_Event &);

extern bool isSpace(char);

extern void logSDLError(const char *);

template <typename T, typename... Args> static inline T *allocate(Args &&...);

template <typename T> static inline void deallocate(T *const);

extern int DefaultPort;

extern size_t MaxPacketSize;
} // namespace TemStream

#include "TemStreamConfig.h"

#include "allocator.hpp"
#include "memoryStream.hpp"

#include "colors.hpp"
#include "logger.hpp"

#include "addrinfo.hpp"

#include "peerInformation.hpp"

#include "message.hpp"
#include "socket.hpp"

#include "address.hpp"

#include "peer.hpp"

#include "audio.hpp"

#include "clientPeer.hpp"
#include "serverPeer.hpp"

#include "streamDisplay.hpp"

#include "workQueue.hpp"

#include "gui.hpp"
#include "query.hpp"

namespace TemStream
{
class LogMutex
{
  private:
	static Map<std::thread::id, size_t> threads;
	Mutex &m;
	const char *name;
	size_t id;

  public:
	LogMutex(Mutex &, const char *);
	~LogMutex();
};
} // namespace TemStream