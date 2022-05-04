#include <main.hpp>

namespace TemStream
{
const char *banList = nullptr;
Configuration::Configuration()
	: access(), address(), name("Server"), startTime(static_cast<int64_t>(time(nullptr))), maxClients(UINT32_MAX),
	  maxMessageSize(MB(1)), serverType(ServerType::UnknownServerType), record(false)
{
}
Configuration::~Configuration()
{
}
#define SET_TYPE(ShortArg, LongArg, s)                                                                                 \
	if (strcasecmp("-" #ShortArg, argv[i]) == 0 || strcasecmp("--" #LongArg, argv[i]) == 0)                            \
	{                                                                                                                  \
		configuration.serverType = ServerType::s;                                                                      \
		++i;                                                                                                           \
		continue;                                                                                                      \
	}
Configuration loadConfiguration(const int argc, const char **argv)
{
	Configuration configuration;
	for (int i = 1; i < argc;)
	{
		if (strcasecmp("-R", argv[i]) == 0 || strcasecmp("--record", argv[i]) == 0)
		{
			configuration.record = true;
			++i;
			continue;
		}
		SET_TYPE(L, link, Link);
		SET_TYPE(T, text, Text);
		SET_TYPE(I, image, Image);
		SET_TYPE(A, audio, Audio);
		SET_TYPE(V, video, Video);
		if (i >= argc - 1)
		{
			continue;
		}
		if (strcasecmp("-B", argv[i]) == 0 || strcasecmp("--banned", argv[i]) == 0)
		{
			std::ifstream file(argv[i + 1]);
			if (!file.is_open())
			{
				std::string message = "Failed to open file: ";
				message += argv[i + 1];
				throw std::invalid_argument(std::move(message));
			}
			configuration.access.banList = true;
			configuration.access.members.clear();
			for (String line; std::getline(file, line);)
			{
				configuration.access.members.emplace(std::move(line));
			}
			banList = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcasecmp("-AL", argv[i]) == 0 || strcasecmp("--allowed", argv[i]) == 0)
		{
			std::ifstream file(argv[i + 1]);
			if (!file.is_open())
			{
				std::string message = "Failed to open file: ";
				message += argv[i + 1];
				throw std::invalid_argument(std::move(message));
			}
			configuration.access.banList = false;
			configuration.access.members.clear();
			for (String line; std::getline(file, line);)
			{
				if (line.empty())
				{
					continue;
				}
				configuration.access.members.emplace(std::move(line));
			}
			banList = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcasecmp("-H", argv[i]) == 0 || strcasecmp("--hostname", argv[i]) == 0)
		{
			configuration.address.hostname = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcasecmp("-P", argv[i]) == 0 || strcasecmp("--port", argv[i]) == 0)
		{
			configuration.address.port = atoi(argv[i + 1]);
			i += 2;
			continue;
		}
		if (strcasecmp("-N", argv[i]) == 0 || strcasecmp("--name", argv[i]) == 0)
		{
			configuration.name = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcasecmp("-M", argv[i]) == 0 || strcasecmp("--memory", argv[i]) == 0)
		{
			// memory already handled
			i += 2;
			continue;
		}
		if (strcasecmp("-MC", argv[i]) == 0 || strcasecmp("--max-clients", argv[i]) == 0)
		{
			configuration.maxClients = static_cast<uint32_t>(atoi(argv[i + 1]));
			i += 2;
			continue;
		}
		if (strcasecmp("-MS", argv[i]) == 0 || strcasecmp("--max-message-size", argv[i]) == 0)
		{
			configuration.maxMessageSize = static_cast<uint32_t>(atoi(argv[i + 1]));
			i += 2;
			continue;
		}
		std::string err("Unexpected argument: ");
		err += argv[i];
		throw std::invalid_argument(std::move(err));
	}
	if (configuration.valid())
	{
		return configuration;
	}

	throw std::invalid_argument("Unknown server type");
}
void saveConfiguration(const Configuration &c)
{
	if (banList == nullptr)
	{
		return;
	}

	std::ofstream file(banList);
	if (!file.is_open())
	{
		return;
	}

	for (const auto &member : c.access.members)
	{
		file << member << std::endl;
	}
}
bool Configuration::valid() const
{
	return validServerType(serverType);
}
} // namespace TemStream