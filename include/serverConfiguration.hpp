#pragma once

#include <main.hpp>

namespace TemStream
{
struct Configuration
{
	Address address;
	String name;
	int64_t startTime;
	uint32_t maxClients;
	uint32_t maxMessageSize;
	uint32_t streamType;
	bool record;

	Configuration();
	virtual ~Configuration();

	friend std::ostream &operator<<(std::ostream &os, const Configuration &configuration)
	{
		os << "Address: " << configuration.address << "\nName: " << configuration.name
		   << "\nMax Clients: " << configuration.maxClients << '\n';
		printMemory(os, "Max Message Size", configuration.maxMessageSize)
			<< "\nRecording: " << (configuration.record ? "Yes" : "No");
		return os;
	}
};
} // namespace TemStream