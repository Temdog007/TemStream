#pragma once

#include <main.hpp>

namespace TemStream
{
enum ServerType : uint8_t
{
	Unknown = 0,
	Link,
	Text,
	Image,
	Audio,
	Video,
	Count
};
extern const char *ServerTypeStrings[ServerType::Count];
extern std::ostream &operator<<(std::ostream &, ServerType);
struct Configuration
{
	Address address;
	String name;
	int64_t startTime;
	uint32_t maxClients;
	uint32_t maxMessageSize;
	ServerType streamType;
	bool record;

	Configuration();
	virtual ~Configuration();

	bool valid() const;

	friend std::ostream &operator<<(std::ostream &os, const Configuration &configuration)
	{
		os << "Address: " << configuration.address << "\nName: " << configuration.name
		   << "\nStream type: " << configuration.streamType << "\nMax Clients: " << configuration.maxClients << '\n';
		printMemory(os, "Max Message Size", configuration.maxMessageSize)
			<< "\nRecording: " << (configuration.record ? "Yes" : "No");
		return os;
	}
};
} // namespace TemStream