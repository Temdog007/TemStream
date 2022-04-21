#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
using Text = String;
using Image = std::variant<std::monostate, uint64_t, Bytes>;
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
struct Video
{
	Bytes bytes;
	template <class Archive> void save(Archive &ar) const
	{
		ar(bytes);
	}
	template <class Archive> void load(Archive &ar)
	{
		ar(bytes);
	}
};
struct Audio
{
	Bytes bytes;
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
using PeerInformationList = List<PeerInformation>;
EMPTY_MESSAGE(GetStreams);
using Streams = Map<Message::Source, Stream>;
using Payload = std::variant<Text, Image, Video, Audio, PeerInformation, RequestPeers, PeerInformationList,
							 StreamUpdate, GetStreams, Streams, GetSubscriptions, Subscriptions>;

#define MESSAGE_HANDLER_FUNCTIONS(RVAL)                                                                                \
	RVAL operator()(Message::Text &);                                                                                  \
	RVAL operator()(Message::Image &);                                                                                 \
	RVAL operator()(Message::Audio &);                                                                                 \
	RVAL operator()(Message::Video &);                                                                                 \
	RVAL operator()(PeerInformation &);                                                                                \
	RVAL operator()(Message::RequestPeers &);                                                                          \
	RVAL operator()(Message::PeerInformationList &);                                                                   \
	RVAL operator()(Message::StreamUpdate &);                                                                          \
	RVAL operator()(Message::GetStreams &);                                                                            \
	RVAL operator()(Message::Streams &);                                                                               \
	RVAL operator()(Message::GetSubscriptions &);                                                                      \
	RVAL operator()(Message::Subscriptions &)

struct Packet
{
	Payload payload;
	Source source;
	List<String> trail;

	template <class Archive> void save(Archive &ar) const
	{
		ar(payload, source, trail);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(payload, source, trail);
	}
};
} // namespace Message
} // namespace TemStream
