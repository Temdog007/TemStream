/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <main.hpp>

#if __unix__
#include <dlfcn.h>
#else
#define dlopen LoadLibrary
#define dlsym GetProcAddress
#define dlclose FreeLibrary
#endif

#define LOAD_LIBRARY(m, X)                                                                                             \
	configuration.m = reinterpret_cast<X>(dlsym(configuration.handle, #X));                                            \
	if (configuration.m == nullptr)                                                                                    \
	{                                                                                                                  \
		throw std::runtime_error("Failed to load authentication function: " #X);                                       \
	}

namespace TemStream
{
const char *banList = nullptr;
Configuration::Configuration()
	: access(), address(), name("Server"), startTime(static_cast<int64_t>(time(nullptr))), handle(nullptr),
	  verifyToken(nullptr), verifyUsernameAndPassword(nullptr), messageRateInSeconds(0), maxClients(UINT32_MAX),
	  maxMessageSize(MB(1)), serverType(ServerType::UnknownServerType), record(false)
{
}
Configuration::~Configuration()
{
	if (handle != nullptr)
	{
		verifyToken = nullptr;
		verifyUsernameAndPassword = nullptr;
		dlclose(handle);
		handle = nullptr;
	}
}
bool Configuration::valid() const
{
	return validServerType(serverType) && (ssl.has_value() ? (!ssl->cert.empty() && !ssl->key.empty()) : true);
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
		SET_TYPE(C, chat, Chat);
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
		if (strcasecmp("-AU", argv[i]) == 0 || strcasecmp("--authentication", argv[i]) == 0)
		{
#if __unix__
			configuration.handle = dlopen(argv[i + 1], RTLD_LAZY);
#else
			configuration.handle = dlopen(argv[i + 1]);
#endif
			if (configuration.handle == nullptr)
			{
				perror("dlopen");
				throw std::runtime_error("Failed to open authentication library");
			}
			LOAD_LIBRARY(verifyToken, VerifyToken);
			LOAD_LIBRARY(verifyUsernameAndPassword, VerifyUsernameAndPassword);
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
		if (strcasecmp("-MR", argv[i]) == 0 || strcasecmp("--message-rate", argv[i]) == 0)
		{
			configuration.messageRateInSeconds = static_cast<uint32_t>(atoi(argv[i + 1]));
			i += 2;
			continue;
		}
		if (strcasecmp("-CT", argv[i]) == 0 || strcasecmp("--certificate", argv[i]) == 0)
		{
			if (!configuration.ssl)
			{
				configuration.ssl = SSLConfig();
			}
			configuration.ssl->cert = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcasecmp("-K", argv[i]) == 0 || strcasecmp("--key", argv[i]) == 0)
		{
			if (!configuration.ssl)
			{
				configuration.ssl = SSLConfig();
			}
			configuration.ssl->key = argv[i + 1];
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
void saveBanList(const char *filename, const Set<String> &members)
{
	std::ofstream file(filename);
	if (!file.is_open())
	{
		return;
	}

	for (const auto &member : members)
	{
		file << member << std::endl;
	}
}
void saveConfiguration(const Configuration &c)
{
	if (banList != nullptr)
	{
		saveBanList(banList, c.access.members);
	}
	else if (!c.access.members.empty())
	{
		String newList = c.name;
		if (c.access.banList)
		{
			newList += "_ban_list.txt";
		}
		else
		{
			newList += "_allowed_list.txt";
		}
		saveBanList(newList.c_str(), c.access.members);
	}
}
std::ostream &operator<<(std::ostream &os, const Configuration &configuration)
{
	os << "Address: " << configuration.address << "\nName: " << configuration.name
	   << "\nStream type: " << configuration.serverType << "\nAccess: " << configuration.access
	   << "\nMax Clients: " << configuration.maxClients
	   << "\nMessage Rate (in seconds): " << configuration.messageRateInSeconds << '\n';
	printMemory(os, "Max Message Size", configuration.maxMessageSize)
		<< "\nRecording: " << (configuration.record ? "Yes" : "No") << "\nAuthentication: " << configuration.handle;
	if (configuration.ssl)
	{
		os << "\nCertificate: " << configuration.ssl->cert << "\nKey: " << configuration.ssl->key;
	}
	return os;
}
Message::Source Configuration::getSource() const
{
	Message::Source source;
	source.address = address;
	source.serverName = name;
	return source;
}
} // namespace TemStream