#include <main.hpp>

namespace TemStream
{
PeerInformation ServerPeer::serverInformation;
Mutex ServerPeer::peersMutex;
List<std::weak_ptr<ServerPeer>> ServerPeer::peers;

bool ServerPeer::peerExists(const PeerInformation &info)
{
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerPeer> ptr = iter->lock())
		{
			if ((*ptr).getInfo() == info)
			{
				return true;
			}
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
	return false;
}
void ServerPeer::sendToAllPeers(MessagePacket &&packet)
{
	MemoryStream m;
	cereal::PortableBinaryOutputArchive ar(m);
	ar(packet);

	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerPeer> ptr = iter->lock())
		{
			// Don't send packet to author
			if ((*ptr).getInfo().name == packet.source.author)
			{
				++iter;
				continue;
			}
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

void ServerPeer::runPeerConnection(shared_ptr<ServerPeer> &&peer)
{
	*logger << "Handling connection: " << peer->getAddress() << std::endl;
	{
		LOCK(peersMutex);
		peers.emplace_back(peer);
	}
	{
		MessagePacket packet;
		packet.message = ServerPeer::serverInformation;
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
	*logger << "Ending connection: " << peer->getAddress() << std::endl;
	--runningThreads;
}
int ServerPeer::run(const int argc, const char **argv)
{
	logger = tem_unique<ConsoleLogger>();
	TemStream::initialLogs();

	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = NULL;
	const char *port = "10000";
	ServerPeer::serverInformation.name = "Server";
	ServerPeer::serverInformation.isServer = true;

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
			ServerPeer::serverInformation.name = argv[i + 1];
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

		auto s = tem_unique<TcpSocket>(newfd);

		std::array<char, INET6_ADDRSTRLEN> str;
		uint16_t port;
		if (s->getIpAndPort(str, port))
		{
			printf("New connection: %s:%u\n", str.data(), port);

			++runningThreads;
			Address address(str.data(), port);
			auto peer = tem_shared<ServerPeer>(address, std::move(s));
			std::thread thread(runPeerConnection, std::move(peer));
			thread.detach();
		}
	}

	result = EXIT_SUCCESS;

end:
	::close(fd);
	logger->AddInfo("Ending server");
	while (runningThreads > 0)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}
	logger = nullptr;
	return result;
}
ServerPeer::ServerPeer(const Address &address, unique_ptr<Socket> s)
	: Peer(address, std::move(s)), informationAcquired(false)
{
}
ServerPeer::~ServerPeer()
{
}
bool ServerPeer::handlePacket(MessagePacket &&packet)
{
	return ServerMessageHandler(*this, std::move(packet))();
}
ServerMessageHandler::ServerMessageHandler(ServerPeer &server, MessagePacket &&packet)
	: server(server), packet(std::move(packet))
{
}
ServerMessageHandler::~ServerMessageHandler()
{
}
bool ServerMessageHandler::operator()()
{
	return std::visit(*this, packet.message);
}
bool ServerMessageHandler::processCurrentMessage()
{
	// All messages from another server should have an author and destination
	if (packet.source.empty())
	{
		return false;
	}

	// If connected to a client, author should match peer name
	if (!server.info.isServer && packet.source.author != server.info.name)
	{
		return false;
	}

	// Don't send packet if server has already received it
	auto iter = std::find(packet.trail.begin(), packet.trail.end(), ServerPeer::serverInformation.name);
	if (iter != packet.trail.end())
	{
		// Stay connected
		return true;
	}

	packet.trail.push_back(ServerPeer::serverInformation.name);
	server.sendToAllPeers(std::move(packet));
	return true;
}
#define CHECK_INFO(X)                                                                                                  \
	if (!server.gotInfo())                                                                                             \
	{                                                                                                                  \
		logger->AddError("Got " #X " from peer before getting their information");                                     \
		return false;                                                                                                  \
	} // namespace TemStream
bool ServerMessageHandler::operator()(TextMessage &)
{
	CHECK_INFO(TextMessage)
	return processCurrentMessage();
}
bool ServerMessageHandler::operator()(ImageMessage &)
{
	CHECK_INFO(ImageMessage)
	return processCurrentMessage();
}
bool ServerMessageHandler::operator()(VideoMessage &)
{
	CHECK_INFO(VideoMessage)
	return processCurrentMessage();
}
bool ServerMessageHandler::operator()(AudioMessage &)
{
	CHECK_INFO(AudioMessage)
	return processCurrentMessage();
}
bool ServerMessageHandler::operator()(PeerInformationList &)
{
	return true;
}
bool ServerMessageHandler::operator()(RequestPeers &)
{
	CHECK_INFO(RequestPeers)
	return true;
}
bool ServerMessageHandler::operator()(PeerInformation &info)
{
	if (server.informationAcquired)
	{
		(*logger)(Logger::Error) << "Peer sent information more than once" << std::endl;
		return false;
	}
	if (ServerPeer::peerExists(info))
	{
		(*logger)(Logger::Error) << "Duplicate peer " << info << " attempted to connect" << std::endl;
		return false;
	}
	server.info = info;
	server.informationAcquired = true;
	*logger << "Information from peer " << server.address << " -> " << info << std::endl;
	return true;
}
} // namespace TemStream