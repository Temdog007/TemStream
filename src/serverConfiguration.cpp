#include <main.hpp>

namespace TemStream
{
Configuration::Configuration()
	: address(), name("Server"), startTime(static_cast<int64_t>(time(nullptr))), maxClients(UINT32_MAX),
	  maxMessageSize(MB(1)), record(false)
{
}
Configuration::~Configuration()
{
}
Configuration loadConfiguration(const int argc, const char **argv)
{
	Configuration configuration;
	for (int i = 1; i < argc;)
	{
		if (strcmp("-R", argv[i]) == 0 || strcmp("--record", argv[i]) == 0)
		{
			configuration.record = true;
			++i;
			continue;
		}
		if (i >= argc - 1)
		{
			continue;
		}
		if (strcmp("-T", argv[i]) == 0 || strcmp("--type", argv[i]) == 0)
		{
			configuration.streamType = static_cast<uint32_t>(strtoul(argv[i + 1], nullptr, 10));
			i += 2;
			continue;
		}
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
		if (strcmp("-M", argv[i]) == 0 || strcmp("--memory", argv[i]) == 0)
		{
			// memory already handled
			i += 2;
			continue;
		}
		if (strcmp("-MC", argv[i]) == 0 || strcmp("--max-clients", argv[i]) == 0)
		{
			configuration.maxClients = static_cast<uint32_t>(atoi(argv[i + 1]));
			i += 2;
			continue;
		}
		if (strcmp("-MS", argv[i]) == 0 || strcmp("--max-message-size", argv[i]) == 0)
		{
			configuration.maxMessageSize = static_cast<uint32_t>(atoi(argv[i + 1]));
			i += 2;
			continue;
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