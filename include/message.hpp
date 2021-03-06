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

#include <main.hpp>

namespace TemStream
{
#define WRITE_STRING_TO_STRING_ARRAY(Name, X) Name##Strings[(uint32_t)Name::X] = #X

enum class ServerType : uint8_t
{
	UnknownServerType = 0u,
	Link,
	Text,
	Chat,
	Image,
	Audio,
	Video,
	Count
};
constexpr uint8_t ServerTypeCount()
{
	return static_cast<uint8_t>(ServerType::Count);
}
extern const char *ServerTypeStrings[ServerTypeCount()];
extern std::ostream &operator<<(std::ostream &, ServerType);
constexpr bool validServerType(const ServerType type)
{
	return (uint32_t)ServerType::UnknownServerType < (uint32_t)type && (uint32_t)type < ServerTypeCount();
}

enum class PeerFlags : uint32_t
{
	None = 0u,
	WriteAccess = 1u << 0u,
	ReplayAccess = 1u << 1u,
	Moderator = 1u << 2u,
	Owner = 1u << 3u
};
constexpr bool peerFlagsOverlap(const PeerFlags a, const PeerFlags b)
{
	return ((uint32_t)a & (uint32_t)b) != 0;
}
extern std::ostream &operator<<(std::ostream &, const PeerFlags);
constexpr PeerFlags operator|(const PeerFlags a, const PeerFlags b)
{
	return static_cast<PeerFlags>(static_cast<int>(a) | static_cast<int>(b));
}
struct PeerInformation
{
	String name;
	PeerFlags flags;

	PeerInformation &swap(PeerInformation &login) noexcept
	{
		std::swap(name, login.name);
		std::swap(flags, login.flags);
		return *this;
	}

	constexpr bool is(const PeerFlags f) const
	{
		return peerFlagsOverlap(flags, f);
	}

	constexpr bool isOwner() const
	{
		return is(PeerFlags::Owner);
	}

	constexpr bool hasWriteAccess() const
	{
		return is(PeerFlags::Owner | PeerFlags::WriteAccess);
	}

	constexpr bool hasReplayAccess() const
	{
		return is(PeerFlags::Owner | PeerFlags::ReplayAccess);
	}

	constexpr bool isModerator() const
	{
		return is(PeerFlags::Owner | PeerFlags::Moderator);
	}

	template <class Archive> void save(Archive &ar) const
	{
		ar(name, flags);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(name, flags);
	}

	friend std::ostream &operator<<(std::ostream &os, const PeerInformation &info)
	{
		os << info.name << " " << info.flags;
		return os;
	}
};
namespace Message
{
#define EMPTY_MESSAGE(Name)                                                                                            \
	struct Name                                                                                                        \
	{                                                                                                                  \
		template <class Archive> void save(Archive &) const                                                            \
		{                                                                                                              \
		}                                                                                                              \
		template <class Archive> void load(Archive &)                                                                  \
		{                                                                                                              \
		}                                                                                                              \
	}
/**
 * A random GUID that will always be used in a valid header
 */
extern const Guid MagicGuid;
/**
 * The header will always be sent before an actual message. The header will contain a magic guid to verify a valid
 * header and the size of the incoming message
 */
struct Header
{
	Guid id;
	uint64_t size;

	template <class Archive> void save(Archive &ar) const
	{
		ar(id, size);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(id, size);
	}

	friend std::ostream &operator<<(std::ostream &, const Header &);
};
/**
 * Message for Text stream
 */
using Text = String;
using UsernameAndPassword = std::pair<String, String>;
using Credentials = std::variant<String, UsernameAndPassword>;
struct VerifyLogin
{
	String serverName;
	PeerInformation peerInformation;
	ServerType serverType;
	uint32_t sendRate;

	template <class Archive> void save(Archive &ar) const
	{
		ar(serverName, peerInformation, serverType, sendRate);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(serverName, peerInformation, serverType, sendRate);
	}
	friend std::ostream &operator<<(std::ostream &os, const VerifyLogin &login)
	{
		os << "Server: " << login.serverName << "; Type: " << login.serverType
		   << "; Peer Information: " << login.peerInformation;
		if (login.sendRate != 0)
		{
			os << "; Message sent rate (in seconds): " << login.sendRate;
		}
		return os;
	}
};
template <class S> struct BaseServerLink
{
	BaseAddress<S> address;
	S name;
	ServerType type;
	BaseServerLink() : address(), name(), type()
	{
	}
	template <class U> BaseServerLink(const BaseServerLink<U> &a) : address(a.address), name(a.name), type(a.type)
	{
	}
	template <class U>
	BaseServerLink(BaseServerLink<U> &&a) : address(std::move(a.address)), name(std::move(a.name)), type(a.type)
	{
	}
	~BaseServerLink()
	{
	}
	template <class Archive> void save(Archive &ar) const
	{
		ar(address, name, type);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(address, name, type);
	}
};
using ServerLink = BaseServerLink<String>;
using ServerLinks = List<ServerLink>;
using LargeFile = std::variant<std::monostate, uint64_t, ByteList>;
/**
 * Message for Image stream
 */
struct Image
{
	LargeFile largeFile;
	template <class Archive> void save(Archive &ar) const
	{
		ar(largeFile);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(largeFile);
	}
};

struct Frame
{
	uint16_t width;
	uint16_t height;
	ByteList bytes;
	template <class Archive> void save(Archive &ar) const
	{
		ar(width, height, bytes);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(width, height, bytes);
	}
};
/**
 * Message for Chat stream
 */
struct Chat
{
	String author;
	String message;
	int64_t timestamp;
	template <class Archive> void save(Archive &ar) const
	{
		ar(author, message, timestamp);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(author, message, timestamp);
	}
};
/**
 * Message for Video stream
 */
using Video = std::variant<Frame, LargeFile>;
/**
 * Message for Audio stream
 */
struct Audio
{
	ByteList bytes;
	template <class Archive> void save(Archive &ar) const
	{
		ar(bytes);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(bytes);
	}
};
EMPTY_MESSAGE(RequestServerInformation);
struct ServerInformation
{
	List<PeerInformation> peers;
	Set<String> banList;
	template <class Archive> void save(Archive &ar) const
	{
		ar(peers, banList);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(peers, banList);
	}
};
struct BanUser
{
	String name;
	template <class Archive> void save(Archive &ar) const
	{
		ar(name);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(name);
	}
};
struct GetReplay
{
	int64_t timestamp;
	template <class Archive> void save(Archive &ar) const
	{
		ar(timestamp);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(timestamp);
	}
};
struct Replay
{
	String message;
	template <class Archive> void save(Archive &ar) const
	{
		ar(message);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(message);
	}
};
EMPTY_MESSAGE(NoReplay);
struct TimeRange
{
	int64_t start;
	int64_t end;
	template <class Archive> void save(Archive &ar) const
	{
		ar(start, end);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(start, end);
	}
};
extern std::ostream &operator<<(std::ostream &, const TimeRange &);
EMPTY_MESSAGE(GetTimeRange);
using Payload = std::variant<std::monostate, Credentials, VerifyLogin, Text, Chat, ServerLinks, Image, Video, Audio,
							 RequestServerInformation, ServerInformation, BanUser, GetReplay, NoReplay, Replay,
							 TimeRange, GetTimeRange>;

#define MESSAGE_HANDLER_FUNCTIONS(RVAL)                                                                                \
	RVAL operator()(std::monostate);                                                                                   \
	RVAL operator()(Message::Text &);                                                                                  \
	RVAL operator()(Message::Chat &);                                                                                  \
	RVAL operator()(Message::ServerLinks &);                                                                           \
	RVAL operator()(Message::Credentials &);                                                                           \
	RVAL operator()(Message::VerifyLogin &);                                                                           \
	RVAL operator()(Message::Image &);                                                                                 \
	RVAL operator()(Message::Audio &);                                                                                 \
	RVAL operator()(Message::Video &);                                                                                 \
	RVAL operator()(Message::RequestServerInformation);                                                                \
	RVAL operator()(Message::ServerInformation &);                                                                     \
	RVAL operator()(Message::BanUser &);                                                                               \
	RVAL operator()(Message::Replay &);                                                                                \
	RVAL operator()(Message::GetReplay);                                                                               \
	RVAL operator()(Message::NoReplay);                                                                                \
	RVAL operator()(Message::TimeRange &);                                                                             \
	RVAL operator()(Message::GetTimeRange)

struct Packet
{
	Payload payload;
	Source source;

	template <class Archive> void save(Archive &ar) const
	{
		ar(payload, source);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(payload, source);
	}
};
extern std::ostream &operator<<(std::ostream &, const Packet &);

/**
 * Get a maximum of MAX_FILE_CHUNK bytes from an iterator
 *
 * @param start Initial iterator
 * @param end Ending iterator
 *
 * @return The bytes
 */
template <typename Iterator> static ByteList getByteChunk(Iterator &start, Iterator end)
{
	ByteList bytes(MAX_FILE_CHUNK);
	while (start != end)
	{
		bytes.append(*start);
		++start;
		if (bytes.size() >= MAX_FILE_CHUNK)
		{
			return bytes;
		}
	}
	return bytes;
}

/**
 * Create a series of LargeFile data streams and call a function with each LargeFile data
 *
 * @param start
 * @param end
 * @param size size of the data
 * @param func callback
 */
template <typename Iterator>
static void prepareLargeBytes(Iterator start, Iterator end, const uint64_t size,
							  const std::function<void(LargeFile &&)> &func)
{
	{
		LargeFile lf = size;
		func(std::move(lf));
	}
	while (start != end)
	{
		ByteList bytes = getByteChunk(start, end);
		if (bytes.empty())
		{
			break;
		}
		LargeFile lf = std::move(bytes);
		func(std::move(lf));
	}
	{
		LargeFile lf = std::monostate{};
		func(std::move(lf));
	}
}
extern void prepareLargeBytes(std::ifstream &, const std::function<void(LargeFile &&)> &);
extern void prepareLargeBytes(const ByteList &, const std::function<void(LargeFile &&)> &);
} // namespace Message
using MessagePackets = List<Message::Packet>;
constexpr size_t ServerTypeToIndex(const ServerType t)
{
	switch (t)
	{
	case ServerType::Text:
		return variant_index<Message::Payload, Message::Text>();
	case ServerType::Chat:
		return variant_index<Message::Payload, Message::Chat>();
	case ServerType::Audio:
		return variant_index<Message::Payload, Message::Audio>();
	case ServerType::Image:
		return variant_index<Message::Payload, Message::Image>();
	case ServerType::Video:
		return variant_index<Message::Payload, Message::Video>();
	case ServerType::Link:
		return variant_index<Message::Payload, Message::ServerLinks>();
	default:
		throw std::runtime_error("Invalid server type");
	}
}
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::PeerInformation>
{
	std::size_t operator()(const TemStream::PeerInformation &info) const
	{
		std::size_t value = hash<TemStream::String>()(info.name);
		TemStream::hash_combine(value, info.flags);
		return value;
	}
};
template <> struct hash<TemStream::Message::VerifyLogin>
{
	std::size_t operator()(const TemStream::Message::VerifyLogin &l) const
	{
		std::size_t value = hash<TemStream::String>()(l.serverName);
		TemStream::hash_combine(value, l.serverType);
		TemStream::hash_combine(value, l.peerInformation);
		return value;
	}
};
} // namespace std