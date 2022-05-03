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
	Image,
	Audio,
	Video,
	ServerTypeCount
};
extern const char *ServerTypeStrings[ServerType::ServerTypeCount];
extern std::ostream &operator<<(std::ostream &, ServerType);
extern bool validServerType(const ServerType);

enum PeerType : uint8_t
{
	InvalidPeerType = 0u,
	Consumer,
	Producer,
	Admin,
	PeerTypeCount
};
extern const char *PeerTypeStrings[PeerType::PeerTypeCount];
extern std::ostream &operator<<(std::ostream &, const PeerType);
extern bool validPeerType(PeerType);
struct PeerInformation
{
	String name;
	PeerType type;
	PeerInformation &swap(PeerInformation &login)
	{
		std::swap(name, login.name);
		std::swap(type, login.type);
		return *this;
	}

	constexpr bool hasWriteAccess() const
	{
		switch (type)
		{
		case PeerType::Consumer:
		case PeerType::Admin:
			return true;
		default:
			return false;
		}
	}
	template <class Archive> void save(Archive &ar) const
	{
		ar(name, type);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(name, type);
	}

	friend std::ostream &operator<<(std::ostream &os, const PeerInformation &info)
	{
		os << info.name << " (" << info.type << ")";
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

	VerifyLogin &swap(VerifyLogin &login)
	{
		std::swap(serverName, login.serverName);
		peerInformation.swap(login.peerInformation);
		std::swap(serverType, login.serverType);
		return *this;
	}
	template <class Archive> void save(Archive &ar) const
	{
		ar(serverName, peerInformation, serverType);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(serverName, peerInformation, serverType);
	}
	friend std::ostream &operator<<(std::ostream &os, const VerifyLogin &login)
	{
		os << "Server: " << login.serverName << "; Type: " << login.serverType
		   << "; Peer Information: " << login.peerInformation << std::endl;
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
EMPTY_MESSAGE(RequestPeers);
struct PeerList
{
	StringList peers;
	template <class Archive> void save(Archive &ar) const
	{
		ar(peers);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(peers);
	}
};
using Payload = std::variant<std::monostate, Credentials, VerifyLogin, Text, ServerLinks, Image, Video, Audio,
							 RequestPeers, PeerList>;

#define MESSAGE_HANDLER_FUNCTIONS(RVAL)                                                                                \
	RVAL operator()(std::monostate);                                                                                   \
	RVAL operator()(Message::Text &);                                                                                  \
	RVAL operator()(Message::ServerLinks &);                                                                           \
	RVAL operator()(Message::Credentials &);                                                                           \
	RVAL operator()(Message::VerifyLogin &);                                                                           \
	RVAL operator()(Message::Image &);                                                                                 \
	RVAL operator()(Message::Audio &);                                                                                 \
	RVAL operator()(Message::Video &);                                                                                 \
	RVAL operator()(Message::RequestPeers &);                                                                          \
	RVAL operator()(Message::PeerList &)

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
