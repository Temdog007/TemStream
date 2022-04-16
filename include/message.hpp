#pragma once

#include <main.hpp>

namespace TemStream
{
struct VideoMessage
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
struct AudioMessage
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
struct RequestPeers
{
	template <class Archive> void save(Archive &ar) const
	{
		(void)ar;
	}
	template <class Archive> void serialize(Archive &ar)
	{
		(void)ar;
	}
};
using TextMessage = std::string;
using ImageMessage = std::variant<bool, Bytes>;
using PeerInformationList = std::vector<PeerInformation>;
using Message = std::variant<RequestPeers, TextMessage, ImageMessage, VideoMessage, AudioMessage, PeerInformation,
							 PeerInformationList>;

struct MessageSource
{
	std::string author;
	std::string destination;

	bool operator==(const MessageSource &s) const
	{
		return author == s.author && destination == s.destination;
	}

	template <class Archive> void save(Archive &ar) const
	{
		ar(author, destination);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(author, destination);
	}
};

struct MessagePacket
{
	Message message;
	MessageSource source;
	std::vector<std::string> trail;

	template <class Archive> void save(Archive &ar) const
	{
		ar(message, source, trail);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(message, source, trail);
	}
};

struct MessagePacketHandler
{
	const MessagePacket *currentPacket;

	virtual bool operator()(const TextMessage &) = 0;
	virtual bool operator()(const ImageMessage &) = 0;
	virtual bool operator()(const VideoMessage &) = 0;
	virtual bool operator()(const AudioMessage &) = 0;
	virtual bool operator()(const PeerInformation &) = 0;
	virtual bool operator()(const PeerInformationList &) = 0;
	virtual bool operator()(const RequestPeers &) = 0;
};
} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::MessageSource>
{
	std::size_t operator()(const TemStream::MessageSource &source) const
	{
		return hash<string>()(source.author) ^ hash<string>()(source.destination);
	}
};
} // namespace std