/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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
