#pragma once

#include <main.hpp>

namespace TemStream
{
struct VideoMessage
{
	Bytes bytes;
	template <class Archive> void serialize(Archive &ar)
	{
		ar(bytes);
	}
};
struct AudioMessage
{
	Bytes bytes;
	template <class Archive> void serialize(Archive &ar)
	{
		ar(bytes);
	}
};
struct RequestPeers
{
	template <class Archive> void serialize(Archive &ar)
	{
	}
};
using TextMessage = std::string;
using ImageMessage = std::variant<bool, Bytes>;
using PeerInformationList = std::vector<PeerInformation>;
using Message = std::variant<RequestPeers, TextMessage, ImageMessage, VideoMessage, AudioMessage, PeerInformationList>;

struct MessagePacket
{
	Message message;
	std::string author;
	std::vector<std::string> trail;

	template <class Archive> void serialize(Archive &ar)
	{
		ar(message, author, trail);
	}
};

struct MessagePacketHandler
{
	const MessagePacket *currentPacket;

	virtual bool operator()(const TextMessage &) = 0;
	virtual bool operator()(const ImageMessage &) = 0;
	virtual bool operator()(const VideoMessage &) = 0;
	virtual bool operator()(const AudioMessage &) = 0;
	virtual bool operator()(const PeerInformationList &) = 0;
	virtual bool operator()(const RequestPeers &) = 0;
};
} // namespace TemStream