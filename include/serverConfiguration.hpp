#pragma once

#include <main.hpp>

namespace TemStream
{
struct Configuration
{
	Access access;
	Address address;
	String name;
	int64_t startTime;
	uint32_t messageRateInSeconds;
	uint32_t maxClients;
	uint32_t maxMessageSize;
	ServerType serverType;
	bool record;

	Configuration();
	~Configuration();

	bool valid() const;

	friend std::ostream &operator<<(std::ostream &os, const Configuration &configuration)
	{
		os << "Address: " << configuration.address << "\nName: " << configuration.name
		   << "\nStream type: " << configuration.serverType << "\nAccess: " << configuration.access
		   << "\nMax Clients: " << configuration.maxClients
		   << "\nMessage Rate (in seconds): " << configuration.messageRateInSeconds << '\n';
		printMemory(os, "Max Message Size", configuration.maxMessageSize)
			<< "\nRecording: " << (configuration.record ? "Yes" : "No");
		return os;
	}
};
} // namespace TemStream