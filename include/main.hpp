#pragma once

#include <arpa/inet.h>
#include <inttypes.h>
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

#include <algorithm>
#include <array>
#include <atomic>
#include <codecvt>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <list>
#include <locale>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

namespace fs = std::filesystem;

#include <cereal/cereal.hpp>
#include <cereal/types/array.hpp>
#include <cereal/types/optional.hpp>
#include <cereal/types/string.hpp>
#include <cereal/types/tuple.hpp>
#include <cereal/types/unordered_map.hpp>
#include <cereal/types/unordered_set.hpp>
#include <cereal/types/variant.hpp>
#include <cereal/types/vector.hpp>

#include <cereal/archives/binary.hpp>
#include <cereal/archives/json.hpp>
#include <cereal/archives/portable_binary.hpp>

#if TEMSTREAM_USE_OPENCV
#include <opencv4/opencv2/opencv.hpp>
#endif

#define TEMSTREAM_USE_BASE64 false

namespace std
{
template <class Archive, typename T1, typename T2> void save(Archive &archive, const std::pair<T1, T2> &pair)
{
	archive(pair.first, pair.second);
}

template <class Archive, typename T1, typename T2> void load(Archive &archive, std::pair<T1, T2> &pair)
{
	archive(pair.first, pair.second);
}
} // namespace std

#if !TEMSTREAM_SERVER
#include <SDL.h>

#include <SDL_image.h>

#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_sdlrenderer.h>
#include <imgui_stdlib.h>

#include <opus.h>

namespace TemStream
{
extern const char *VideoExtension;
enum TemStreamEvent : int32_t
{
	ReloadFont = 0xabcd,
	SendSingleMessagePacket,
	SendMessagePackets,
	HandleMessagePacket,
	HandleMessagePackets,
	SetQueryData,
	SetSurfaceToStreamDisplay,
	AddAudio,
	HandleFrame
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

extern bool isTTF(const char *);
extern bool isJpeg(const char *);
extern bool isXPM(const char *);

extern bool isImage(const char *);

extern void SetWindowMinSize(SDL_Window *window);

extern bool tryPushEvent(SDL_Event &);

extern void logSDLError(const char *);

} // namespace TemStream
#endif

#define KB(X) (X * 1024UL)
#define MB(X) (KB(X) * 1024UL)
#define GB(X) (MB(X) * 1024UL)

#define MAX_FILE_CHUNK KB(64)

#define _DEBUG !NDEBUG
#if _DEBUG
#include <cxxabi.h>
#define LOG_MESSAGE_TYPE false
#else
#define LOG_MESSAGE_TYPE false
#endif

#define THREADS_AVAILABLE (!__EMSCRIPTEN__ || !SDL_THREADS_DISABLED)

namespace TemStream
{
using Mutex = std::recursive_mutex;
#define LOCK(M) std::lock_guard<Mutex> mutexLockGuard(M)
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

extern bool openSocket(int &, const char *hostname, const char *port, const bool isServer, const bool isTcp);

extern bool sendData(int, const void *, size_t);

extern PollState pollSocket(const int fd, const int timeout, const int events);

extern bool appDone;

extern void initialLogs();

extern bool isSpace(char);

extern int64_t getTimestamp();

extern const char *getExtension(const char *filename);

using TimePoint = std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::duration<double, std::nano>>;

class Configuration;
extern Configuration loadConfiguration(int, const char **);
extern void saveConfiguration(const Configuration &);
extern int runApp(Configuration &);

extern const char *ApplicationPath;

template <typename T> T lerp(T min, T max, float percent)
{
	return min + (max - min) * percent;
}

template <typename T> auto toMoveIterator(T &&t)
{
	auto begin = std::make_move_iterator(t.begin());
	auto end = std::make_move_iterator(t.end());
	return std::make_pair(begin, end);
}

template <typename VariantType, typename T, std::size_t index = 0> constexpr std::size_t variant_index()
{
	static_assert(std::variant_size_v<VariantType> > index, "Type not found in variant");
	if constexpr (index == std::variant_size_v<VariantType>)
	{
		return index;
	}
	else if constexpr (std::is_same_v<std::variant_alternative_t<index, VariantType>, T>)
	{
		return index;
	}
	else
	{
		return variant_index<VariantType, T, index + 1>();
	}
}

template <typename T> void cleanSwap(T &t)
{
	T newT;
	t.swap(newT);
}

} // namespace TemStream

#include "TemStreamConfig.h"

#include "guid.hpp"

#include "allocator.hpp"

#include "byteList.hpp"

#include "fixedSizeList.hpp"

#include "base64.hpp"

#include "memoryStream.hpp"

#include "logger.hpp"
#include "windowProcess.hpp"

#include "access.hpp"

#include "addrinfo.hpp"

#include "address.hpp"

#include "messageSource.hpp"

#include "socket.hpp"

#include "message.hpp"

#include "concurrentMap.hpp"
#include "concurrentQueue.hpp"

#include "connection.hpp"

#include "time.hpp"

#if TEMSTREAM_SERVER
#include "serverConfiguration.hpp"
#include "serverConnection.hpp"
#else
#include "work.hpp"

#include "sdl.hpp"

#include "audioSource.hpp"
#include "videoSource.hpp"

#include "clientConfiguration.hpp"
#include "clientConnection.hpp"

#include "streamDisplay.hpp"

#include "gui.hpp"
#include "query.hpp"

#endif
