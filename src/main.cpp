#include <main.hpp>

using namespace TemStream;

bool TemStream::appDone = false;
std::atomic<int32_t> TemStream::runningThreads = 0;
int TemStream::DefaultPort = 10000;
size_t TemStream::MaxPacketSize = MB(1);

void signalHandler(int s)
{
	switch (s)
	{
	case SIGINT:
		TemStream::appDone = true;
		puts("Received end signal");
		break;
	default:
		break;
	}
}

int main(int argc, const char **argv)
{
	printf("TemStream v%d.%d.%d\n", TemStream_VERSION_MAJOR, TemStream_VERSION_MINOR, TemStream_VERSION_PATCH);
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
			return runServer(argc, argv);
		}
	}
	return runGui();
}
