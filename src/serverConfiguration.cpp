#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: address(), name("Server"), maxMessageSize(MB(1)), maxStreamsPerClient(10u), maxTotalStreams(1024u),
	  recordStreams(false)
{
}
Configuration::~Configuration()
{
}
PeerInformation Configuration::getInfo() const
{
	PeerInformation i;
	i.name = name;
	i.isServer = true;
	return i;
}

Configuration loadConfiguration(const int argc, const char **argv)
{
	Configuration configuration;
	for (int i = 1; i < argc;)
	{
		if (strcmp("-R", argv[i]) == 0 || strcmp("--record", argv[i]) == 0)
		{
			configuration.recordStreams = true;
			++i;
			continue;
		}
		if (i < argc - 1)
		{
			if (strcmp("-H", argv[i]) == 0 || strcmp("--hostname", argv[i]) == 0)
			{
				configuration.address.hostname = argv[i + 1];
				i += 2;
				continue;
			}
			if (strcmp("-P", argv[i]) == 0 || strcmp("--port", argv[i]) == 0)
			{
				configuration.address.port = atoi(argv[i + 1]);
				i += 2;
				continue;
			}
			if (strcmp("-N", argv[i]) == 0 || strcmp("--name", argv[i]) == 0)
			{
				configuration.name = argv[i + 1];
				i += 2;
				continue;
			}

			if (strcmp("-MS", argv[i]) == 0 || strcmp("--max-message-size", argv[i]) == 0)
			{
				configuration.maxMessageSize = static_cast<uint32_t>(atoi(argv[i + 1]));
				i += 2;
				continue;
			}
			if (strcmp("-MSC", argv[i]) == 0 || strcmp("--max-streams-per-client", argv[i]) == 0)
			{
				configuration.maxStreamsPerClient = static_cast<uint32_t>(atoi(argv[i + 1]));
				i += 2;
				continue;
			}
			if (strcmp("-MTS", argv[i]) == 0 || strcmp("--max-total-streams", argv[i]) == 0)
			{
				configuration.maxTotalStreams = static_cast<uint32_t>(atoi(argv[i + 1]));
				i += 2;
				continue;
			}
		}
		std::string err("Unexpected argument: ");
		err += argv[i];
		throw std::invalid_argument(std::move(err));
	}
	return configuration;
}
void saveConfiguration(const Configuration &)
{
}
} // namespace TemStream