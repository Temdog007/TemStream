#pragma once

#include <main.hpp>

namespace TemStream
{

enum PeerType : uint16_t
{
	Consumer,
	Producer,
	Server
};

extern const char *PeerTypeToString(PeerType);

struct PeerInformation
{
	std::string name;
	PeerType type;

	template <class Archive> void serialize(Archive &archive)
	{
		archive(name, type);
	}
};
} // namespace TemStream