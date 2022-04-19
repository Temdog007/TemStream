#pragma once

#include <main.hpp>

namespace TemStream
{
struct PeerInformation
{
	String name;
	bool isServer;

	bool operator==(const PeerInformation &info) const
	{
		return isServer == info.isServer && name == info.name;
	}
	bool operator!=(const PeerInformation &info) const
	{
		return !(*this == info);
	}

	friend std::ostream &operator<<(std::ostream &os, const PeerInformation &info)
	{
		os << "Name: " << info.name << '(' << (info.isServer ? "Server" : "Client") << ')';
		return os;
	}

	template <class Archive> void serialize(Archive &archive)
	{
		archive(name, isServer);
	}
};
} // namespace TemStream