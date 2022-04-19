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
using TextMessage = String;
using ImageMessage = std::variant<bool, Bytes>;
using PeerInformationList = List<PeerInformation>;
using Message = std::variant<RequestPeers, TextMessage, ImageMessage, VideoMessage, AudioMessage, PeerInformation,
							 PeerInformationList>;

struct MessageSource
{
	String author;
	String destination;

	bool operator==(const MessageSource &s) const
	{
		return author == s.author && destination == s.destination;
	}

	bool operator!=(const MessageSource &s) const
	{
		return !(*this == s);
	}

	bool empty() const
	{
		return author.empty() && destination.empty();
	}

	template <class Archive> void save(Archive &ar) const
	{
		ar(author, destination);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(author, destination);
	}

	template <const size_t N> int print(std::array<char, N> &arr) const
	{
		return snprintf(arr.data(), sizeof(arr), "%s (%s)", destination.c_str(), author.c_str());
	}

	explicit operator String() const
	{
		String s(destination);
		s += " (";
		s += author;
		s += ')';
		return s;
	}

	friend std::ostream &operator<<(std::ostream &os, const MessageSource &s)
	{
		os << s.destination << " (" << s.author << ')';
		return os;
	}
};

struct MessagePacket
{
	Message message;
	MessageSource source;
	List<String> trail;

	template <class Archive> void save(Archive &ar) const
	{
		ar(message, source, trail);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(message, source, trail);
	}
};
} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::MessageSource>
{
	std::size_t operator()(const TemStream::MessageSource &source) const
	{
		std::size_t value = hash<TemStream::String>()(source.author);
		TemStream::hash_combine(value, source.destination);
		return value;
	}
};
} // namespace std