#pragma once

#include <main.hpp>

namespace TemStream
{
using TextMessage = std::string;
using ImageMessage = std::variant<bool, Bytes>;
using VideoMessage = Bytes;
using AudioMessage = Bytes;
using Message = std::variant<TextMessage, ImageMessage, VideoMessage, AudioMessage>;

struct MessagePacket
{
	Message message;
	std::string source;

	template <class Archive> void serialize(Archive &ar)
	{
		ar(message, source);
	}
};
} // namespace TemStream