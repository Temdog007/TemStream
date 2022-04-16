#include <main.hpp>

namespace TemStream
{
bool getIpAndPort(const int fd, std::array<char, INET6_ADDRSTRLEN> &str, uint16_t &port)
{
	struct sockaddr_storage addr;
	socklen_t size = sizeof(addr);
	if (getpeername(fd, (struct sockaddr *)&addr, &size) < 0)
	{
		perror("getpeername");
		return false;
	}

	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		inet_ntop(addr.ss_family, &s->sin_addr, str.data(), str.size());
		port = ntohs(s->sin_port);
	}
	else
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		inet_ntop(addr.ss_family, &s->sin6_addr, str.data(), str.size());
		port = ntohs(s->sin6_port);
	}
	return true;
}
void runPeerConnection(int fd)
{
	std::array<char, INET6_ADDRSTRLEN> str;
	uint16_t port = 0;
	if (!getIpAndPort(fd, str, port))
	{
		::close(fd);
		return;
	}
	printf("Handling connection: %s:%u\n", str.data(), port);

	ServerPeer peer(fd);
	while (!appDone)
	{
		if (!peer.readData(1000))
		{
			break;
		}
	}

	printf("Ending connection: %s:%u\n", str.data(), port);
}
int runServer(const int argc, const char **argv)
{
	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = NULL;
	const char *port = "10000";

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

	printf("Connecting to %s:%s\n", hostname == nullptr ? "any" : hostname, port);
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

		std::array<char, INET6_ADDRSTRLEN> str;
		uint16_t port;
		if (getIpAndPort(newfd, str, port))
		{
			printf("New connection: %s:%u\n", str.data(), port);
		}

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
bool ServerPeer::operator()(const PeerInformation &info)
{
	this->info = info;
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