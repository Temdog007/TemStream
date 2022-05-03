#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
struct Source
{
	String peer;
	String server;

	bool operator==(const Source &s) const
	{
		return peer == s.peer && server == s.server;
	}

	bool operator!=(const Source &s) const
	{
		return !(*this == s);
	}

	bool empty() const
	{
		return peer.empty() && server.empty();
	}

	template <class Archive> void save(Archive &ar) const
	{
		ar(peer, server);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(peer, server);
	}

	template <const size_t N> int print(std::array<char, N> &arr, const size_t offset = 0) const
	{
		return snprintf(arr.data() + offset, sizeof(arr) - offset, "%s (%s)", server.c_str(), peer.c_str());
	}

	explicit operator String() const
	{
		String s(server);
		s += " (";
		s += peer;
		s += ')';
		return s;
	}

	friend std::ostream &operator<<(std::ostream &os, const Source &s)
	{
		os << s.server << " (" << s.peer << ')';
		return os;
	}
};
} // namespace Message
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::Message::Source>
{
	std::size_t operator()(const TemStream::Message::Source &source) const
	{
		std::size_t value = hash<TemStream::String>()(source.peer);
		TemStream::hash_combine(value, source.server);
		return value;
	}
};
} // namespace std
