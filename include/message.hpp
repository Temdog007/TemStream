#pragma once

#include <main.hpp>

namespace TemStream
{
#define WRITE_STRING_TO_STRING_ARRAY(Name, X) Name##Strings[Name::X] = #X

enum ServerType : uint8_t
{
	UnknownServerType = 0u,
	Link,
	Text,
	Chat,
	Image,
	Audio,
	Video,
	ServerTypeCount
};
extern const char *ServerTypeStrings[ServerType::ServerTypeCount];
extern std::ostream &operator<<(std::ostream &, ServerType);
extern bool validServerType(const ServerType);

enum PeerFlags : uint32_t
{
	None = 0u,
	WriteAccess = 1u << 0u,
	ReplayAccess = 1u << 1u,
	Moderator = 1u << 2u,
	Owner = 1u << 3u
};
constexpr bool peerFlagsOverlap(const PeerFlags a, const PeerFlags b)
{
	return (a & b) != 0;
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
		return is(Owner);
	}

	constexpr bool hasWriteAccess() const
	{
		return is(Owner | WriteAccess);
	}

	constexpr bool hasReplayAccess() const
	{
		return is(Owner | ReplayAccess);
	}

	constexpr bool isModerator() const
	{
		return is(Owner | Moderator);
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
extern const Guid MagicGuid;
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
using Video = std::variant<Frame, LargeFile>;
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
using Payload = std::variant<std::monostate, Credentials, VerifyLogin, Text, Chat, ServerLinks, Image, Video, Audio,
							 RequestServerInformation, ServerInformation, BanUser>;

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
	RVAL operator()(Message::RequestServerInformation &);                                                              \
	RVAL operator()(Message::ServerInformation &);                                                                     \
	RVAL operator()(Message::BanUser &)

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