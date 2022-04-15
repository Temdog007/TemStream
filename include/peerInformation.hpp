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

struct PeerInformation
{
	std::string name;
	PeerType role;

	bool isConsumer() const
	{
		return (role & PeerType::Consumer) != 0;
	}

	bool isProducer() const
	{
		return (role & PeerType::Producer) != 0;
	}

	bool isServer() const
	{
		return (role & PeerType::Server) != 0;
	}

	template <class Archive> void serialize(Archive &archive)
	{
		archive(name, role);
	}
};
} // namespace TemStream