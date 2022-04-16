#pragma once

#include <main.hpp>

namespace TemStream
{
struct PeerInformation
{
	std::string name;
	bool isServer;

	template <class Archive> void serialize(Archive &archive)
	{
		archive(name, isServer);
	}
};
} // namespace TemStream