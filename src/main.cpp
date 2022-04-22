#include <main.hpp>

using namespace TemStream;

bool TemStream::appDone = false;
std::atomic<int32_t> TemStream::runningThreads = 0;
AllocatorData TemStream::globalAllocatorData;
unique_ptr<Logger> TemStream::logger = nullptr;

void signalHandler(int s);
void parseMemory(int, const char **, size_t);

int main(const int argc, const char **argv)
{
	{
		struct sigaction action;
		action.sa_handler = &signalHandler;
		sigfillset(&action.sa_mask);
		if (sigaction(SIGINT, &action, nullptr) == -1)
		{
			perror("sigaction");
			return EXIT_FAILURE;
		}
	}
	const size_t defaultMemory =
#if TEMSTREAM_SERVER
		8
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

void TemStream::initialLogs()
{
	const char *AppName =
#if TEMSTREAM_SERVER
		"TemStream Server"
#else
		"TemStream"
#endif
		;
	*logger << AppName << ' ' << TemStream_VERSION_MAJOR << '.' << TemStream_VERSION_MINOR << '.'
			<< TemStream_VERSION_PATCH << std::endl;
	(*logger)(Logger::Trace) << "Debug mode" << std::endl;
	(*logger) << "Using " << globalAllocatorData.getTotal() / MB(1) << " MB" << std::endl;
#if USE_CUSTOM_ALLOCATOR
	(*logger)(Logger::Trace) << "Using custom allocator" << std::endl;
#endif
}

void signalHandler(int s)
{
	switch (s)
	{
	case SIGINT:
		TemStream::appDone = true;
		logger->AddInfo("Received end signal");
		break;
	default:
		break;
	}
}

void parseMemory(const int argc, const char **argv, size_t size)
{
	for (int i = 1; i < argc - 1; i += 2)
	{
		if (strcmp("-M", argv[i]) == 0 || strcmp("--memory", argv[i]) == 0)
		{
			size = static_cast<size_t>(strtoull(argv[i + 1], nullptr, 10));
			break;
		}
	}

	globalAllocatorData.init(size * MB(1));
}