#include <main.hpp>

using namespace TemStream;

bool TemStream::appDone = false;

int main(int argc, const char **argv)
{
	printf("TemStream v%d.%d.%d\n", TemStream_VERSION_MAJOR, TemStream_VERSION_MINOR, TemStream_VERSION_PATCH);
	for (int i = 1; i < argc; ++i)
	{
		if (strcmp(argv[i], "-S") == 0 || strcmp(argv[i], "--server") == 0)
		{
			return runServer(argc, argv);
		}
	}
	return runGui();
}
