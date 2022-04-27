#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
struct Source
{
	String author;
	String destination;

	bool operator==(const Source &s) const
	{
		return author == s.author && destination == s.destination;
	}

	bool operator!=(const Source &s) const
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

	template <const size_t N> int print(std::array<char, N> &arr, const size_t offset = 0) const
	{
		return snprintf(arr.data() + offset, sizeof(arr) - offset, "%s (%s)", destination.c_str(), author.c_str());
	}

	explicit operator String() const
	{
		String s(destination);
		s += " (";
		s += author;
		s += ')';
		return s;
	}

	friend std::ostream &operator<<(std::ostream &os, const Source &s)
	{
		os << s.destination << " (" << s.author << ')';
		return os;
	}
};
} // namespace Message
extern bool stopVideoStream(const Message::Source &);
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::Message::Source>
{
	std::size_t operator()(const TemStream::Message::Source &source) const
	{
		std::size_t value = hash<TemStream::String>()(source.author);
		TemStream::hash_combine(value, source.destination);
		return value;
	}
};
} // namespace std
