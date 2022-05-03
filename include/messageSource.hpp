#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
struct Source
{
	Address address;
	String serverName;

	bool operator==(const Source &s) const
	{
		return serverName == s.serverName && address == s.address;
	}

	bool operator!=(const Source &s) const
	{
		return !(*this == s);
	}

	bool empty() const
	{
		return serverName.empty();
	}

	template <class Archive> void save(Archive &ar) const
	{
		ar(address, serverName);
	}

	template <class Archive> void load(Archive &ar)
	{
		ar(address, serverName);
	}

	template <const size_t N> int print(std::array<char, N> &arr, const size_t offset = 0) const
	{
		const String s = static_cast<String>(*this);
		return snprintf(arr.data() + offset, sizeof(arr) - offset, "%s", s.c_str());
	}

	explicit operator String() const
	{
		StringStream ss;
		ss << *this;
		return ss.str();
	}

	friend std::ostream &operator<<(std::ostream &os, const Source &s)
	{
		os << s.serverName << " (" << s.address << ')';
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
		std::size_t value = hash<TemStream::Address>()(source.address);
		TemStream::hash_combine(value, source.serverName);
		return value;
	}
};
} // namespace std
