/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#pragma once

#if WIN32
#define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2def.h>
#include <ws2tcpip.h>
#define poll WSAPoll
#define SOL_TCP IPPROTO_TCP
#define strcasecmp _stricmp
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
static inline void closesocket(const int fd)
{
	close(fd);
}
typedef int SOCKET;
constexpr SOCKET INVALID_SOCKET = -1;
#endif

#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
#include <utility>
#include <variant>
#include <vector>

#include <openssl/err.h>
#include <openssl/ssl.h>

#if __linux__
using TimePoint = std::chrono::time_point<std::chrono::_V2::system_clock, std::chrono::duration<double, std::nano>>;
#else
using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
#endif

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

#if TEMSTREAM_SERVER || TEMSTREAM_CHAT_TEST
#define TEMSTREAM_HAS_GUI false
#else
#define TEMSTREAM_HAS_GUI true
#endif

#if TEMSTREAM_HAS_GUI
#include <opencv2/opencv.hpp>
#endif

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

#if TEMSTREAM_HAS_GUI
#include <SDL.h>

#if WIN32
#include <SDL2/SDL_image.h>
#else
#include <SDL_image.h>
#endif

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

/**
 * Check if filename extension is .ttf
 *
 * @param filename
 *
 * @return True if extension is .ttf
 */
extern bool isTTF(const char *);

/**
 * Check if filename extension is .jpg or jpeg
 *
 * @param filename
 *
 * @return True if extension is .jpg or .jpeg
 */
extern bool isJpeg(const char *);

/**
 * Check if filename extension is .xpm
 *
 * @param filename
 *
 * @return True if extension is .xpm
 */
extern bool isXPM(const char *);

/**
 * Check if filename points to a file. Might try and load the image if it cannot be determined from the file extension
 *
 * @param filename
 *
 * @return True if is image
 */
extern bool isImage(const char *);

/**
 * Set the size of the next window to a quarter of the size of the window if it is the first time the window has been
 * opened
 *
 * @param window
 */
extern void SetWindowMinSize(SDL_Window *window);

/**
 * Call SDL_PushEvent
 *
 * @param event
 *
 * @return True if the event was added to the event queue successfully
 */
extern bool tryPushEvent(SDL_Event &);

/**
 * Log error from SDL
 *
 * @param message
 */
extern void logSDLError(const char *);

} // namespace TemStream
#endif

#define KB(X) (X * 1024UL)
#define MB(X) (KB(X) * 1024UL)
#define GB(X) (MB(X) * 1024UL)

#define MAX_FILE_CHUNK KB(64)

#ifndef _DEBUG
#define _DEBUG !NDEBUG
#endif

#define THREADS_AVAILABLE (!__EMSCRIPTEN__ || !SDL_THREADS_DISABLED)

namespace TemStream
{
using Mutex = std::recursive_mutex;
#define LOCK(M) std::lock_guard<Mutex> mutexLockGuard(M)
enum class PollState
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

enum class SocketType
{
	None,
	Client,
	Server
};

/**
 * @brief Open a socket with these parameters
 *
 * @param socket [out] Will contain the newly created socket on success
 * @param hostname
 * @param port
 * @param socketType
 * @param isTcp If true, establish a TCP connection. Else, UDP.
 *
 * @return True if successful
 */
extern bool openSocket(SOCKET &, const char *hostname, const char *port, const SocketType, const bool isTcp);

/**
 * Send data through socket
 *
 * @param socket
 * @param data
 * @param length
 *
 * @return True if all data was sent
 */
extern bool sendData(SOCKET, const void *, size_t);

/**
 * Check the state of the socket
 *
 * @param socket
 * @param timeout How long to poll the socket
 * @param events Which events to poll for
 *
 * @return the poll state
 */
extern PollState pollSocket(const SOCKET socket, const int timeout, const int events);

/**
 * Global flag to state when the application should no longer be running
 */
extern bool appDone;

/**
 * Log the application name, the ersion, and the memory consumption
 */
extern void initialLogs();

/**
 * Check if character is a white space character
 *
 * @param c
 *
 * @return True if character is a white space character
 */
extern bool isSpace(char);

/**
 * Get the number of milliseconds since epoch from the current time
 *
 * @return the number of milliseconds since epoch from the current time
 */
extern int64_t getTimestamp();

/**
 * Get the extension of the file
 *
 * @return the file extension
 */
extern const char *getExtension(const char *filename);

struct Configuration;
extern Configuration loadConfiguration(int, const char **);
extern void saveConfiguration(const Configuration &);

/**
 * The main function for the application
 *
 * @param configuration
 *
 * @return return code
 */
extern int runApp(Configuration &);

/**
 * The first argument passed to the application
 *
 * i.e.
 * ApplicationPath = argv[0]
 * if the main function declaration is int main(int argc, char* argv[])
 */
extern const char *ApplicationPath;

template <typename T> T lerp(T min, T max, float percent)
{
	return min + static_cast<T>((max - min) * percent);
}

template <typename T> T randomBetween(T min, T max)
{
	return lerp(min, max, static_cast<float>(rand()) / RAND_MAX);
}

template <typename T> auto toMoveIterator(T &&t)
{
	auto begin = std::make_move_iterator(t.begin());
	auto end = std::make_move_iterator(t.end());
	return std::make_pair(begin, end);
}

/**
 * Get the index of a type in a varaint
 *
 * (i.e. variant<int, float, string>, int = 0, float = 1, string = 2)
 */
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

namespace std
{
template <typename T> ostream &operator<<(ostream &os, const optional<T> &value)
{
	if (value.has_value())
	{
		os << *value;
	}
	else
	{
		os << "<No value>";
	}
	return os;
}
} // namespace std

#if TEMSTREAM_SERVER
#include "serverConfiguration.hpp"
#include "serverConnection.hpp"
#elif TEMSTREAM_CHAT_TEST
#include "chatTester.hpp"
#else
#include "work.hpp"

#include "colors.hpp"

#include "sdl.hpp"

#include "audioSource.hpp"
#include "videoSource.hpp"

#include "clientConfiguration.hpp"
#include "clientConnection.hpp"

#include "streamDisplay.hpp"

#include "gui.hpp"
#include "query.hpp"

#endif
