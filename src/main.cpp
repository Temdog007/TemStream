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

#if TEMSTREAM_HAS_GUI
#include <SDL2/SDL_main.h>
#endif

using namespace TemStream;

bool TemStream::appDone = false;
AllocatorData TemStream::globalAllocatorData;
unique_ptr<Logger> TemStream::logger = nullptr;
const char *TemStream::ApplicationPath = nullptr;

#if __unix__
void signalHandler(int s);
#endif
void parseMemory(int, const char **, size_t);

#if WIN32
struct WSAHandler
{
	~WSAHandler()
	{
		WSACleanup();
	}
};
#endif

#if __cplusplus
extern "C"
#endif
	int
	main(int argc, char *argv[])
{
	srand(static_cast<uint32_t>(time(nullptr)));

#if WIN32
	{
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;

		/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
		wVersionRequested = MAKEWORD(2, 2);

		err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
		{
			/* Tell the user that we could not find a usable */
			/* Winsock DLL.                                  */
			fprintf(stderr, "WSAStartup failed with error: %d\n", err);
			return EXIT_FAILURE;
		}
	}
	WSAHandler handler;
#endif

	TemStream::ApplicationPath = argv[0];
#if __unix__
	{
		struct sigaction action;
		action.sa_handler = &signalHandler;
		sigfillset(&action.sa_mask);
		if (sigaction(SIGINT, &action, nullptr) == -1 || sigaction(SIGPIPE, &action, nullptr) == -1)
		{
			perror("sigaction");
			return EXIT_FAILURE;
		}
	}
#endif
	try
	{
		const size_t defaultMemory =
#if TEMSTREAM_SERVER
			128
#elif TEMSTREAM_CHAT_TEST
			32
#else
			256
#endif
			;
		parseMemory(argc, (const char **)argv, defaultMemory);
		Configuration configuration = loadConfiguration(argc, (const char **)argv);
		const int result = runApp(configuration);
		saveConfiguration(configuration);
		return result;
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
		return EXIT_FAILURE;
	}
	catch (const std::exception &e)
	{
		fprintf(stderr, "An error occurred: %s\n", e.what());
		return EXIT_FAILURE;
	}
}

void TemStream::initialLogs()
{
	const char *AppName =
#if TEMSTREAM_SERVER
		"TemStream Server"
#elif TEMSTREAM_CHAT_TEST
		"TemStream Chat Test"
#else
		"TemStream"
#endif
		;
	*logger << AppName << ' ' << TemStream_VERSION_MAJOR << '.' << TemStream_VERSION_MINOR << '.'
			<< TemStream_VERSION_PATCH << std::endl;
#if _DEBUG
	(*logger)(Logger::Level::Trace) << "Debug mode" << std::endl;
#endif
	(*logger) << "Using " << printMemory(globalAllocatorData.getTotal()) << std::endl;
#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
	(*logger)(Logger::Level::Trace) << "Using custom allocator" << std::endl;
#endif
}

#if __unix__
void signalHandler(int s)
{
	if (TemStream::appDone)
	{
		return;
	}
	switch (s)
	{
	case SIGINT:
		TemStream::appDone = true;
		logger->AddInfo("Received end signal");
		break;
	case SIGPIPE:
		(*logger)(Logger::Level::Error) << "Broken pipe error occurred" << std::endl;
		break;
	default:
		break;
	}
}
#endif

void parseMemory(const int argc, const char **argv, size_t size)
{
	for (int i = 1; i < argc - 1; ++i)
	{
		if (strcasecmp("-M", argv[i]) == 0 || strcasecmp("--memory", argv[i]) == 0)
		{
			size = static_cast<size_t>(strtoull(argv[i + 1], nullptr, 10));
			break;
		}
	}

	globalAllocatorData.init(size * MB(1));
}