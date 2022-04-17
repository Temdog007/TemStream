#include <main.hpp>

namespace TemStream
{
PeerInformation peerInformation;

void runPeerConnection(const Address address, std::unique_ptr<Socket> s)
{
	ServerPeer peer(address, std::move(s));
	std::array<char, INET6_ADDRSTRLEN> str;
	uint16_t port = 0;
	Bytes bytes;
	if (!peer->getIpAndPort(str, port))
	{
		goto end;
	}
	printf("Handling connection: %s:%u\n", str.data(), port);

	{
		MessagePacket packet;
		packet.message = peerInformation;
		if (!peer->sendPacket(packet))
		{
			goto end;
		}
	}
	while (!appDone)
	{
		if (!peer.readAndHandle(1000))
		{
			break;
		}
	}

end:
	printf("Ending connection: %s:%u\n", str.data(), port);
	--runningThreads;
}
int runServer(const int argc, const char **argv)
{
	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = NULL;
	const char *port = "10000";
	peerInformation.name = "Server";
	peerInformation.isServer = true;

	for (int i = 1; i < argc - 1;)
	{
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
		if (strcmp(argv[i], "-N") == 0 || strcmp(argv[i], "--name") == 0)
		{
			peerInformation.name = argv[i + 1];
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

		auto s = std::make_unique<TcpSocket>(newfd);

		std::array<char, INET6_ADDRSTRLEN> str;
		uint16_t port;
		if (s->getIpAndPort(str, port))
		{
			printf("New connection: %s:%u\n", str.data(), port);

			++runningThreads;
			Address address(str.data(), port);
			std::thread thread(runPeerConnection, address, std::move(s));
			thread.detach();
		}
	}

	result = EXIT_SUCCESS;

end:
	::close(fd);
	puts("Ending server");
	while (runningThreads > 0)
	{
		SDL_Delay(100);
	}
	return result;
}
ServerPeer::ServerPeer(const Address &address, std::unique_ptr<Socket> s)
	: Peer(address, std::move(s)), informationAcquired(false)
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
#define CHECK_INFO(X)                                                                                                  \
	if (!gotInfo())                                                                                                    \
	{                                                                                                                  \
		std::cerr << "Got " << #X << " from peer before getting their information" << std::endl;                       \
		return false;                                                                                                  \
	}
bool ServerPeer::operator()(const TextMessage &)
{
	CHECK_INFO(TextMessage)
	return true;
}
bool ServerPeer::operator()(const ImageMessage &)
{
	CHECK_INFO(ImageMessage)
	return true;
}
bool ServerPeer::operator()(const VideoMessage &)
{
	CHECK_INFO(VideoMessage)
	return true;
}
bool ServerPeer::operator()(const AudioMessage &)
{
	CHECK_INFO(AudioMessage)
	return true;
}
bool ServerPeer::operator()(const PeerInformationList &)
{
	return true;
}
bool ServerPeer::operator()(const RequestPeers &)
{
	CHECK_INFO(RequestPeers)
	return true;
}
bool ServerPeer::operator()(const PeerInformation &info)
{
	this->info = info;
	informationAcquired = true;
	std::cout << "Information for peer " << address << " -> " << info << std::endl;
	return true;
}
} // namespace TemStream