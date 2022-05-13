#include <main.hpp>

#include <SDL2/SDL_main.h>

using namespace TemStream;

bool TemStream::appDone = false;
AllocatorData TemStream::globalAllocatorData;
unique_ptr<Logger> TemStream::logger = nullptr;
const char *TemStream::ApplicationPath = nullptr;

void signalHandler(int s);
void parseMemory(int, const char **, size_t);

extern "C" int main(const int argc, const char **argv)
{
	srand(time(nullptr));

	TemStream::ApplicationPath = argv[0];
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
		parseMemory(argc, argv, defaultMemory);
		Configuration configuration = loadConfiguration(argc, argv);
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