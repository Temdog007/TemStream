#include <main.hpp>

namespace TemStream
{
PeerInformation ServerPeer::serverInformation;
std::mutex peersMutex;
std::vector<std::weak_ptr<ServerPeer>> peers;

void ServerPeer::sendToAllPeers(const MessagePacket &packet)
{
	MemoryStream m;
	cereal::PortableBinaryOutputArchive ar(m);
	ar(packet);

	std::lock_guard<std::mutex> guard(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (std::shared_ptr<ServerPeer> ptr = iter->lock())
		{
			if ((*ptr)->send(m.getData(), m.getSize()))
			{
				++iter;
			}
			else
			{
				iter = peers.erase(iter);
			}
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
}

void ServerPeer::runPeerConnection(std::shared_ptr<ServerPeer> peer)
{
	std::cout << "Handling connection: " << peer->getAddress() << std::endl;
	{
		std::lock_guard<std::mutex> guard(peersMutex);
		peers.emplace_back(peer);
	}
	{
		MessagePacket packet;
		packet.message = serverInformation;
		if (!(*peer)->sendPacket(packet))
		{
			goto end;
		}
	}

	while (!appDone)
	{
		if (!peer->readAndHandle(1000))
		{
			break;
		}
	}

end:
	std::cout << "Ending connection: " << peer->getAddress() << std::endl;
	--runningThreads;
}
int ServerPeer::runServer(const int argc, const char **argv)
{
	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = NULL;
	const char *port = "10000";
	serverInformation.name = "Server";
	serverInformation.isServer = true;

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
			serverInformation.name = argv[i + 1];
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
			auto peer = std::make_shared<ServerPeer>(address, std::move(s));
			std::thread thread(runPeerConnection, std::move(peer));
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

bool ServerPeer::processCurrentMessage() const
{
	// Don't send packet if server has already received it
	auto iter = std::find(currentPacket->trail.begin(), currentPacket->trail.end(), serverInformation.name);
	if (iter != currentPacket->trail.end())
	{
		return true;
	}

	MessagePacket newPacket(*currentPacket);
	if (info.isServer)
	{
		// Messages from another server should have an author
		if (currentPacket->source.author.empty())
		{
			return false;
		}
	}
	else
	{
		// Messages from clients shouldn't write the author
		if (!currentPacket->source.author.empty())
		{
			return false;
		}
		newPacket.source.author = info.name;
	}

	newPacket.trail.push_back(serverInformation.name);
	sendToAllPeers(newPacket);
	return true;
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
	return processCurrentMessage();
}
bool ServerPeer::operator()(const ImageMessage &)
{
	CHECK_INFO(ImageMessage)
	return processCurrentMessage();
}
bool ServerPeer::operator()(const VideoMessage &)
{
	CHECK_INFO(VideoMessage)
	return processCurrentMessage();
}
bool ServerPeer::operator()(const AudioMessage &)
{
	CHECK_INFO(AudioMessage)
	return processCurrentMessage();
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