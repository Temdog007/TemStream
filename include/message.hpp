#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
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
	PeerInformation info;
	VerifyLogin &swap(VerifyLogin &login)
	{
		std::swap(info, login.info);
		return *this;
	}
	template <class Archive> void save(Archive &ar) const
	{
		ar(info);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(info);
	}
};
using Image = std::variant<std::monostate, uint64_t, ByteList>;
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
using Video = std::variant<Frame, ByteList>;
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
struct StreamUpdate
{
	Source source;
	enum
	{
		Create,
		Delete,
		Subscribe,
		Unsubscribe
	} action;
	uint32_t type;

	template <class Archive> void save(Archive &ar) const
	{
		ar(source, action, type);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(source, action, type);
	}

	friend std::ostream &operator<<(std::ostream &os, const StreamUpdate &su)
	{
		os << su.source << "; Action: " << su.action << "; Type: " << su.type;
		return os;
	}
};
EMPTY_MESSAGE(GetSubscriptions);
using Subscriptions = Set<Message::Source>;
EMPTY_MESSAGE(RequestPeers);
using PeerInformationSet = Set<PeerInformation>;
EMPTY_MESSAGE(GetStreams);
using Streams = Map<Message::Source, Stream>;
using Payload =
	std::variant<std::monostate, Text, Credentials, VerifyLogin, Image, Video, Audio, PeerInformation, RequestPeers,
				 PeerInformationSet, StreamUpdate, GetStreams, Streams, GetSubscriptions, Subscriptions>;

#define MESSAGE_HANDLER_FUNCTIONS(RVAL)                                                                                \
	RVAL operator()(std::monostate);                                                                                   \
	RVAL operator()(Message::Text &);                                                                                  \
	RVAL operator()(Message::Credentials &);                                                                           \
	RVAL operator()(Message::VerifyLogin &);                                                                           \
	RVAL operator()(Message::Image &);                                                                                 \
	RVAL operator()(Message::Audio &);                                                                                 \
	RVAL operator()(Message::Video &);                                                                                 \
	RVAL operator()(PeerInformation &);                                                                                \
	RVAL operator()(Message::RequestPeers &);                                                                          \
	RVAL operator()(Message::PeerInformationSet &);                                                                    \
	RVAL operator()(Message::StreamUpdate &);                                                                          \
	RVAL operator()(Message::GetStreams &);                                                                            \
	RVAL operator()(Message::Streams &);                                                                               \
	RVAL operator()(Message::GetSubscriptions &);                                                                      \
	RVAL operator()(Message::Subscriptions &)

struct Packet
{
	Payload payload;
	Source source;
	StringList trail;

	template <class Archive> void save(Archive &ar) const
	{
		ar(payload, source, trail);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(payload, source, trail);
	}
};
template <typename Iterator> static ByteList getImageByteChunk(Iterator &start, Iterator end)
{
	ByteList bytes(MAX_IMAGE_CHUNK);
	while (start != end)
	{
		bytes.append(*start);
		++start;
		if (bytes.size() >= MAX_IMAGE_CHUNK)
		{
			return bytes;
		}
	}
	return bytes;
}
template <typename Iterator>
static void prepareImageBytes(Iterator start, Iterator end, const uint64_t size, const Source &source,
							  const std::function<void(Packet &&)> &func)
{
	{
		Message::Packet packet;
		packet.payload.emplace<Message::Image>(size);
		packet.source = source;
		func(std::move(packet));
	}
	while (start != end)
	{
		ByteList bytes = getImageByteChunk(start, end);
		if (bytes.empty())
		{
			break;
		}
		Message::Packet packet;
		packet.payload.emplace<Message::Image>(std::move(bytes));
		packet.source = source;
		func(std::move(packet));
	}
	{
		Message::Packet packet;
		packet.payload.emplace<Message::Image>(std::monostate{});
		packet.source = source;
		func(std::move(packet));
	}
}
extern void prepareImageBytes(std::ifstream &, const Source &, const std::function<void(Packet &&)> &);
} // namespace Message
} // namespace TemStream
