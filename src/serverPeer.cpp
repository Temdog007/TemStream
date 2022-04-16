#include <main.hpp>

namespace TemStream
{
void runPeerConnection(int fd)
{
	ServerPeer peer(fd);
	while (!appDone)
	{
		appDone = !peer.readData(1000);
	}
}
int runServer(const int argc, const char **argv)
{
	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = nullptr;
	const char *port = nullptr;

	for (int i = 1; i < argc - 1;)
	{
		puts(argv[i]);
		if (strcmp(argv[i], "-H") == 0 || strcmp(argv[i], "--hostname") == 0)
		{
			hostname = argv[i + 1];
			i += 2;
			continue;
		}
		if (strcmp(argv[i], "-P") == 0 || strcmp(argv[i], "--port") == 0)
		{
			port = argv[i + 1];
			i += 2;
			continue;
		}
		++i;
	}

	printf("Connecting to %s:%s\n", hostname == nullptr ? "localhost" : hostname, port ? "any" : port);
	if (!openSocket(fd, hostname, port, true))
	{
		goto end;
	}

	while (!appDone)
	{
		struct pollfd inputfd;
		inputfd.events = POLLIN;
		inputfd.fd = fd;
		inputfd.revents = 0;
		switch (pollSocket(fd, 1000))
		{
		case PollState::GotData:
			break;
		case PollState::Error:
			appDone = true;
			continue;
		default:
			continue;
		}

		struct sockaddr_storage addr;
		socklen_t size = sizeof(addr);
		const int newfd = accept(fd, (struct sockaddr *)&addr, &size);
		if (newfd < 0)
		{
			perror("accept");
			continue;
		}

		char str[INET6_ADDRSTRLEN];
		inet_ntop(addr.ss_family, get_in_addr((struct sockaddr *)&addr), str, sizeof(str));
		printf("New connection: %s\n", str);

		std::thread thread(runPeerConnection, newfd);
		thread.detach();
	}

	result = EXIT_SUCCESS;

end:
	::close(fd);
	puts("Ending server");
	return result;
}
ServerPeer::ServerPeer(int fd) : Peer(fd)
{
}
ServerPeer::~ServerPeer()
{
}
bool ServerPeer::handlePacket(const MessagePacket &packet)
{
	currentPacket = &packet;
	return std::visit(*this, packet.message);
}
bool ServerPeer::operator()(const TextMessage &)
{
	return true;
}
bool ServerPeer::operator()(const ImageMessage &)
{
	return true;
}
bool ServerPeer::operator()(const VideoMessage &)
{
	return true;
}
bool ServerPeer::operator()(const AudioMessage &)
{
	return true;
}
bool ServerPeer::operator()(const PeerInformationList &)
{
	return true;
}
bool ServerPeer::operator()(const RequestPeers &)
{
	return true;
}
} // namespace TemStream