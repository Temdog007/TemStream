#include <main.hpp>

using namespace TemStream;

bool TemStream::appDone = false;
std::atomic<int32_t> TemStream::runningThreads = 0;
int TemStream::DefaultPort = 10000;
size_t TemStream::MaxPacketSize = MB(1);
AllocatorData TemStream::globalAllocatorData;
std::unique_ptr<Logger> TemStream::logger = nullptr;

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
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--server") == 0)
		{
			parseMemory(argc, argv, 8);
			return ServerPeer::runServer(argc, argv);
		}
	}
	parseMemory(argc, argv, 256);
	return runGui();
}

void TemStream::initialLogs()
{
	logger->AddInfo("TemStream v%d.%d.%d\n", TemStream_VERSION_MAJOR, TemStream_VERSION_MINOR, TemStream_VERSION_PATCH);
#if _DEBUG
	logger->AddInfo("Debug mode\n");
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

	// Logger not set yet
	printf("Memory usage: %zu MB\n", size);
	globalAllocatorData.init(size * MB(1));
}