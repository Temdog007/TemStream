#pragma once

#include <main.hpp>

namespace TemStream
{
enum ServerType : uint8_t
{
	Unknown = 0,
	Link,
	Text,
	Image,
	Audio,
	Video,
	Count
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
struct PeerInformation
{
	String name;
	bool writeAccess;
	PeerInformation &swap(PeerInformation &login)
	{
		std::swap(name, login.name);
		std::swap(writeAccess, login.writeAccess);
		return *this;
	}
	template <class Archive> void save(Archive &ar) const
	{
		ar(name, writeAccess);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(name, writeAccess);
	}

	friend std::ostream &operator<<(std::ostream &os, const PeerInformation &info)
	{
		os << info.name << " (Write Access: " << (info.writeAccess ? "Yes" : "No") << ")";
		return os;
	}
};

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
using Payload =
	std::variant<std::monostate, Text, Credentials, VerifyLogin, Image, Video, Audio, RequestPeers, PeerList>;

#define MESSAGE_HANDLER_FUNCTIONS(RVAL)                                                                                \
	RVAL operator()(std::monostate);                                                                                   \
	RVAL operator()(Message::Text &);                                                                                  \
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
} // namespace TemStream
