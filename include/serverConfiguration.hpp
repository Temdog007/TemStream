#pragma once

#include <main.hpp>

#define CONFIGURATION_ARCHIVE(archive)                                                                                 \
	archive(address, name, maxClients, maxMessageSize, maxStreamsPerClient, maxTotalStreams, recordStreams)

namespace TemStream
{
struct Configuration
{
	Address address;
	String name;
	uint32_t maxClients;
	uint32_t maxMessageSize;
	uint32_t maxStreamsPerClient;
	uint32_t maxTotalStreams;
	bool recordStreams;

	Configuration();
	~Configuration();

	template <class Archive> void save(Archive &archive) const
	{
		CONFIGURATION_ARCHIVE(archive);
	}

	template <class Archive> void load(Archive &archive)
	{
		CONFIGURATION_ARCHIVE(archive);
	}

	PeerInformation getInfo() const;

	friend std::ostream &operator<<(std::ostream &os, const Configuration &configuration)
	{
		os << "Address: " << configuration.address << "\nName: " << configuration.name
		   << "\nMax Clients: " << configuration.maxClients << '\n';
		printMemory(os, "Max Message Size", configuration.maxMessageSize)
			<< "\nMax Streams Per Client: " << configuration.maxStreamsPerClient
			<< "\nMax Total Streams: " << configuration.maxTotalStreams
			<< "\nRecording: " << (configuration.recordStreams ? "Yes" : "No");
		return os;
	}
};
} // namespace TemStream