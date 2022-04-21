#include <main.hpp>

namespace TemStream
{
Message::Streams ServerConnection::streams;
PeerInformation ServerConnection::serverInformation;
Mutex ServerConnection::peersMutex;
List<std::weak_ptr<ServerConnection>> ServerConnection::peers;

bool ServerConnection::peerExists(const PeerInformation &info)
{
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerConnection> ptr = iter->lock())
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
bool ServerConnection::sendToAllPeers(Message::Packet &&packet, const Target target)
{
	const bool toServers = (target & Target::Server) != 0;
	const bool toClient = (target & Target::Client) != 0;

	MemoryStream m;
	cereal::PortableBinaryOutputArchive ar(m);
	ar(packet);

	LOCK(peersMutex);
	auto streamIter = ServerConnection::streams.find(packet.source);
	// Stream doesn't exist. So, don't send packet
	if (streamIter == ServerConnection::streams.end())
	{
		return false;
	}
	// If type doesn't match, don't send packet
	if (streamIter->second.getType() != packet.payload.index())
	{
		return false;
	}
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerConnection> ptr = iter->lock())
		{
			// Don't send packet to author
			if ((*ptr).getInfo().name == packet.source.author)
			{
				++iter;
				continue;
			}
			if (ptr->isServer())
			{
				if (!toServers)
				{
					++iter;
					continue;
				}
			}
			else
			{
				if (!toClient)
				{
					++iter;
					continue;
				}
				// If not subscribed, client won't get packet
				auto sub = ptr->subscriptions.find(packet.source);
				if (sub == ptr->subscriptions.end())
				{
					++iter;
					continue;
				}
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
	return true;
}

void ServerConnection::runPeerConnection(shared_ptr<ServerConnection> &&peer)
{
	*logger << "Handling connection: " << peer->getAddress() << std::endl;
	{
		LOCK(peersMutex);
		peers.emplace_back(peer);
	}
	{
		Message::Packet packet;
		packet.payload.emplace<PeerInformation>(ServerConnection::serverInformation);
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
int ServerConnection::run(const int argc, const char **argv)
{
	logger = tem_unique<ConsoleLogger>();
	TemStream::initialLogs();

	int result = EXIT_FAILURE;
	int fd = -1;

	const char *hostname = NULL;
	const char *port = "10000";
	ServerConnection::serverInformation.name = "Server";
	ServerConnection::serverInformation.isServer = true;

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
			ServerConnection::serverInformation.name = argv[i + 1];
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
			auto peer = tem_shared<ServerConnection>(address, std::move(s));
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
ServerConnection::ServerConnection(const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), subscriptions(), informationAcquired(false)
{
}
ServerConnection::~ServerConnection()
{
}
bool ServerConnection::handlePacket(Message::Packet &&packet)
{
	return ServerConnection::MessageHandler(*this, std::move(packet))();
}
ServerConnection::MessageHandler::MessageHandler(ServerConnection &connection, Message::Packet &&packet)
	: connection(connection), packet(std::move(packet))
{
}
ServerConnection::MessageHandler::~MessageHandler()
{
}
bool ServerConnection::MessageHandler::operator()()
{
	return std::visit(*this, packet.payload);
}
bool ServerConnection::MessageHandler::processCurrentMessage(const Target target)
{
	// All messages should have an author and destination
	if (packet.source.empty())
	{
		return false;
	}

	// If connected to a client, author should match peer name
	if (!connection.isServer() && packet.source.author != connection.info.name)
	{
		return false;
	}

	// Stream must exist in stream list
	{
		LOCK(peersMutex);
		if (ServerConnection::streams.find(packet.source) == ServerConnection::streams.end())
		{
			return false;
		}
	}

	// Don't send packet if server has already received it
	auto iter = std::find(packet.trail.begin(), packet.trail.end(), ServerConnection::serverInformation.name);
	if (iter != packet.trail.end())
	{
		// Stay connected
		return true;
	}

	packet.trail.push_back(ServerConnection::serverInformation.name);
	return connection.sendToAllPeers(std::move(packet), target);
}
#define CHECK_INFO(X)                                                                                                  \
	if (!connection.gotInfo())                                                                                         \
	{                                                                                                                  \
		logger->AddError("Got " #X " from peer before getting their information");                                     \
		return false;                                                                                                  \
	} // namespace TemStream
bool ServerConnection::MessageHandler::operator()(Message::Text &)
{
	CHECK_INFO(Message::Text)
	return processCurrentMessage();
}
bool ServerConnection::MessageHandler::operator()(Message::Image &)
{
	CHECK_INFO(Message::Image)
	return processCurrentMessage();
}
bool ServerConnection::MessageHandler::operator()(Message::Video &)
{
	CHECK_INFO(Message::Video)
	return processCurrentMessage();
}
bool ServerConnection::MessageHandler::operator()(Message::Audio &)
{
	CHECK_INFO(Message::Audio)
	return processCurrentMessage();
}
bool ServerConnection::MessageHandler::operator()(PeerInformation &info)
{
	if (connection.informationAcquired)
	{
		(*logger)(Logger::Error) << "Connection sent information more than once" << std::endl;
		return false;
	}
	if (ServerConnection::peerExists(info))
	{
		(*logger)(Logger::Error) << "Duplicate peer " << info << " attempted to connect" << std::endl;
		return false;
	}
	connection.info = info;
	connection.informationAcquired = true;
	*logger << "Information from peer " << connection.address << " -> " << info << std::endl;
	return true;
}
bool ServerConnection::MessageHandler::operator()(Message::PeerInformationList &)
{
	// TODO: Server only. Send current set of peers to clients
	return true;
}
bool ServerConnection::MessageHandler::operator()(Message::RequestPeers &)
{
	CHECK_INFO(Message::RequestPeers)
	// TODO: If from client, send request to other servers.
	//  If from server, send current set of clients
	return true;
}
bool ServerConnection::MessageHandler::operator()(Message::StreamUpdate &su)
{
	CHECK_INFO(Message::StreamUpdate)
	const auto &info = connection.getInfo();
	switch (su.action)
	{
	case Message::StreamUpdate::Create: {
		auto iter = connection.streams.find(su.source);
		if (iter != connection.streams.end())
		{
			// Stream already exists. Nothing to do but don't terminate connection.
			break;
		}
		if (!info.isServer)
		{
			// If client tries to create stream, ensure they are the author
			if (su.source.author != info.name)
			{
				return false;
			}
		}
		connection.streams.emplace(Message::Source(su.source), Stream(su.source, su.type));
		*logger << "Client " << info.name << " created stream " << su.source << std::endl;
	}
	break;
	case Message::StreamUpdate::Delete: {
		if (!info.isServer)
		{
			// If client tries to delete stream, ensure they are the author
			if (su.source.author != info.name)
			{
				return false;
			}
		}

		connection.streams.erase(su.source);
		*logger << "Client " << info.name << " deleted stream " << su.source << std::endl;
	}
	break;
	case Message::StreamUpdate::Subscribe: {
		if (info.isServer)
		{
			// Servers should never try to subscribe to stream
			return false;
		}
		// Clients shouldn't subscribe to their own stream
		if (su.source.author == info.name)
		{
			break;
		}
		connection.subscriptions.emplace(su.source);
		*logger << "Client " << info.name << " subscribed stream " << su.source << std::endl;
		// Don't send message to other servers
		return true;
	}
	break;
	case Message::StreamUpdate::Unsubscribe: {
		if (info.isServer)
		{
			// Servers should never try to unsubscribe to stream
			return false;
		}
		// Clients shouldn't have to unsubscribe to their own stream
		if (su.source.author == info.name)
		{
			break;
		}
		connection.subscriptions.erase(su.source);
		*logger << "Client " << info.name << " unsubscribed stream " << su.source << std::endl;
		// Don't send message to other servers
		return true;
	}
	break;
	default:
		(*logger)(Logger::Error) << "Invalid action " << su.action << std::endl;
		return false;
	}
	return processCurrentMessage(Target::Server);
}
bool ServerConnection::MessageHandler::operator()(Message::GetStreams &)
{
	// Send get streams to servers
	if (!processCurrentMessage(Target::Server))
	{
		return false;
	}

	// Send current streams to all clients
	return sendStreamsToClients();
}
bool ServerConnection::MessageHandler::operator()(Message::Streams &streams)
{
	// Server only. Append to the list of streams and send to clients
	if (!connection.isServer())
	{
		return false;
	}
	connection.streams.insert(streams.begin(), streams.end());
	return sendStreamsToClients();
}
bool ServerConnection::MessageHandler::sendStreamsToClients()
{
	Message::Packet packet;
	packet.source.author = serverInformation.name;
	packet.payload.emplace<Message::Streams>(connection.streams);
	return sendToAllPeers(std::move(packet), Target::Client);
}
bool ServerConnection::MessageHandler::operator()(Message::GetSubscriptions &)
{
	// Send get subscription to servers
	if (!processCurrentMessage(Target::Server))
	{
		return false;
	}

	// Send current subscriptions to clients
	return sendSubscriptionsToClient();
}
bool ServerConnection::MessageHandler::operator()(Message::Subscriptions &subs)
{
	// Server only. Send current subscriptions to clients
	if (!connection.isServer())
	{
		return false;
	}
	connection.subscriptions.insert(subs.begin(), subs.end());
	return sendSubscriptionsToClient();
}
bool ServerConnection::MessageHandler::sendSubscriptionsToClient()
{
	Message::Packet packet;
	packet.source.author = serverInformation.name;
	packet.payload.emplace<Message::Subscriptions>(connection.subscriptions);
	return connection->sendPacket(packet);
}
} // namespace TemStream