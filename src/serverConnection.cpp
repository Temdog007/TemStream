#include <main.hpp>

#define CHECK_INFO(X)                                                                                                  \
	if (!connection.gotInfo())                                                                                         \
	{                                                                                                                  \
		logger->AddError("Got " #X " from peer before getting their information");                                     \
		return false;                                                                                                  \
	} // namespace TemStream
#define BAD_MESSAGE(X)                                                                                                 \
	(*logger)(Logger::Error) << "Client " << connection.getInfo() << " sent invalid message: '" #X "'" << std::endl;   \
	return false
#define PEER_ERROR(str)                                                                                                \
	(*logger)(Logger::Error) << "Error with peer '" << connection.getInfo() << "': " << str << std::endl;              \
	return false

namespace TemStream
{
std::atomic_int32_t ServerConnection::runningThreads = 0;
Message::Streams ServerConnection::streams;
Configuration ServerConnection::configuration;
Mutex ServerConnection::peersMutex;
List<std::weak_ptr<ServerConnection>> ServerConnection::peers;
Message::PeerInformationSet ServerConnection::peersFromOtherServers;

int runApp(Configuration &configuration)
{
	logger = tem_unique<ConsoleLogger>();
	TemStream::initialLogs();

	*logger << configuration << std::endl;

	int result = EXIT_FAILURE;
	int fd = -1;

	ServerConnection::configuration = configuration;

	if (!openSocket(fd, configuration.address, true))
	{
		goto end;
	}

	while (!appDone)
	{
		switch (pollSocket(fd, 1000, POLLIN))
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

		if (ServerConnection::totalPeers() >= configuration.maxClients)
		{
			::close(newfd);
			continue;
		}

		auto s = tem_unique<TcpSocket>(newfd);

		std::array<char, INET6_ADDRSTRLEN> str;
		uint16_t port;
		if (s->getIpAndPort(str, port))
		{
			*logger << "New connection: " << str.data() << ':' << port << std::endl;

			Address address(str.data(), port);
			auto peer = tem_shared<ServerConnection>(address, std::move(s));
			++ServerConnection::runningThreads;
			std::thread thread(ServerConnection::runPeerConnection, std::move(peer));
			thread.detach();
		}
	}

	result = EXIT_SUCCESS;

end:
	::close(fd);
	logger->AddInfo("Ending server");
	while (ServerConnection::runningThreads > 0)
	{
		using namespace std::chrono_literals;
		std::this_thread::sleep_for(100ms);
	}
	logger = nullptr;
	return result;
}
void ServerConnection::runPeerConnection(shared_ptr<ServerConnection> peer)
{
	{
		LOCK(peersMutex);
		peers.emplace_back(peer);
	}
	peer->handleInput();
	--ServerConnection::runningThreads;
}
void ServerConnection::handleInput()
{
	struct ConnectionLog
	{
		ServerConnection &connection;
		ConnectionLog(ServerConnection &connection) : connection(connection)
		{
			*logger << "Handling connection: " << connection.getAddress() << std::endl;
		}
		~ConnectionLog()
		{
			*logger << "Ending connection: " << connection.getAddress() << std::endl;
		}
	};
	ConnectionLog cl(*this);

	maxMessageSize = configuration.maxMessageSize;
	{
		Message::Packet packet;
		packet.payload.emplace<PeerInformation>(ServerConnection::configuration.getInfo());
		if (!mSocket->sendPacket(packet))
		{
			return;
		}
	}

	std::thread thread(&ServerConnection::handleOutput, this);

	while (!appDone && stayConnected)
	{
		if (!readAndHandle(1000))
		{
			break;
		}
	}

	stayConnected = false;
	thread.join();

	{
		LOCK(peersMutex);
		auto &streams = ServerConnection::streams;
		const auto &name = getInfo().name;
		for (auto iter = streams.begin(); iter != streams.end();)
		{
			if (iter->first.author == name)
			{
				iter = streams.erase(iter);
			}
			else
			{
				++iter;
			}
		}
	}
}
void ServerConnection::handleOutput()
{
	using namespace std::chrono_literals;
	while (!appDone && stayConnected)
	{
		auto packet = incomingPackets.pop(100ms);
		if (!packet)
		{
			continue;
		}
		stayConnected = ServerConnection::MessageHandler(*this, std::move(*packet))();
	}
}
bool ServerConnection::sendToPeers(Message::Packet &&packet, const Target target, const bool checkSubscription)
{
	const bool toServers = (target & Target::Server) != 0;
	const bool toClients = (target & Target::Client) != 0;

	MemoryStream m;
	{
		cereal::PortableBinaryOutputArchive ar(m);
		ar(packet);
	}

	LOCK(peersMutex);
	uint32_t expected = 0;
	bool validPayload = true;
	if (checkSubscription && toClients)
	{
		auto stream = ServerConnection::getStream(packet.source);
		if (!stream.has_value())
		{
			(*logger)(Logger::Warning) << "Stream " << packet.source << " doesn't exist" << std::endl;
			// Don't disconnect
			return true;
		}
		// If type doesn't match, don't send packet.
		expected = stream->getType();
		validPayload = stream->getType() == packet.payload.index();
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
				if (!toClients)
				{
					++iter;
					continue;
				}
				// If not subscribed, client won't get packet
				if (checkSubscription)
				{
					auto sub = ptr->subscriptions.find(packet.source);
					if (sub == ptr->subscriptions.end())
					{
						++iter;
						continue;
					}
					if (!validPayload)
					{
						(*logger)(Logger::Error) << "Payload mismatch for stream. Got " << packet.payload.index()
												 << "; Expected " << expected << std::endl;
						return false;
					}
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
std::optional<Stream> ServerConnection::getStream(const Message::Source &source)
{
	LOCK(peersMutex);
	auto iter = ServerConnection::streams.find(source);
	if (iter == ServerConnection::streams.end())
	{
		return std::nullopt;
	}
	return iter->second;
}
std::optional<PeerInformation> ServerConnection::getPeerFromCredentials(Message::Credentials &&credentials)
{
	// TODO: Load dll to handle credentials
	return std::visit(CredentialHandler(nullptr, nullptr), std::move(credentials));
}
size_t ServerConnection::totalStreams()
{
	LOCK(peersMutex);
	return ServerConnection::streams.size();
}
size_t ServerConnection::totalPeers()
{
	LOCK(peersMutex);
	return ServerConnection::peers.size();
}
Message::PeerInformationSet ServerConnection::getPeers()
{
	Message::PeerInformationSet set;
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (auto ptr = iter->lock())
		{
			set.insert(ptr->info);
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
	return set;
}
shared_ptr<ServerConnection> ServerConnection::getPointer() const
{
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (auto ptr = iter->lock())
		{
			if (ptr.get() == this)
			{
				return ptr;
			}
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
	return nullptr;
}
ServerConnection::ServerConnection(const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), incomingPackets(), subscriptions(), stayConnected(true),
	  informationAcquired(false)
{
}
ServerConnection::~ServerConnection()
{
}
bool ServerConnection::handlePacket(Message::Packet &&packet)
{
	incomingPackets.push(std::move(packet));
	return true;
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
bool ServerConnection::MessageHandler::processCurrentMessage(const Target target, const bool checkSubscription)
{
	// All messages should have an author and destination
	if (packet.source.empty())
	{
		PEER_ERROR("Got message with no source");
	}

	// If connected to a client, author should match peer name
	if (!connection.isServer() && packet.source.author != connection.info.name)
	{
		PEER_ERROR("Got message with invalid author");
	}

	// Don't send packet if server has already received it
	auto iter = std::find(packet.trail.begin(), packet.trail.end(), ServerConnection::configuration.name);
	if (iter != packet.trail.end())
	{
		// Stay connected
		return true;
	}

	packet.trail.push_back(ServerConnection::configuration.name);
	return ServerConnection::sendToPeers(std::move(packet), target, checkSubscription);
}
bool ServerConnection::MessageHandler::operator()(std::monostate)
{
	return false;
}
bool ServerConnection::MessageHandler::operator()(Message::Text &)
{
	CHECK_INFO(Message::Text)
	savePayloadIfNedded();
	return processCurrentMessage();
}
bool ServerConnection::MessageHandler::operator()(Message::Image &image)
{
	CHECK_INFO(Message::Image)
	std::visit(ImageSaver(connection, packet.source), image);
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
bool ServerConnection::MessageHandler::operator()(Message::Credentials &credentials)
{
	if (connection.informationAcquired)
	{
		(*logger)(Logger::Error) << "Connection sent information more than once" << std::endl;
		return false;
	}
	auto info = getPeerFromCredentials(std::move(credentials));
	if (!info.has_value() || info->name.empty())
	{
		(*logger)(Logger::Error) << "Invalid credentials sent" << std::endl;
		return false;
	}
	if (ServerConnection::peerExists(*info))
	{
		(*logger)(Logger::Error) << "Duplicate peer " << *info << " attempted to connect" << std::endl;
		return false;
	}
	connection.info = std::move(*info);
	connection.informationAcquired = true;
	*logger << "Peer: " << connection.address << " -> " << connection.info << std::endl;
	{
		Message::Packet packet;
		packet.source.author = configuration.name;
		packet.payload.emplace<Message::VerifyLogin>(Message::VerifyLogin{connection.info});
		if (!connection->sendPacket(packet))
		{
			return false;
		}
	}
	return sendStreamsToClients();
}
bool ServerConnection::MessageHandler::operator()(Message::VerifyLogin &)
{
	BAD_MESSAGE(VerifyLogin);
}
bool ServerConnection::MessageHandler::operator()(PeerInformation &)
{
	BAD_MESSAGE(PeerInformation);
}
bool ServerConnection::MessageHandler::operator()(Message::PeerInformationSet &set)
{
	CHECK_INFO(Message::RequestPeers)
	LOCK(peersMutex);
	if (!connection.isServer())
	{
		BAD_MESSAGE(PeerInformationSet);
	}

	ServerConnection::peersFromOtherServers.insert(set.begin(), set.end());
	return processCurrentMessage(Target::Server, false);
}
bool ServerConnection::MessageHandler::operator()(Message::RequestPeers &)
{
	CHECK_INFO(Message::RequestPeers)
	LOCK(peersMutex);
	// If from client, clear current set and send request to other servers.
	if (connection.isServer())
	{
		peersFromOtherServers.clear();
		return processCurrentMessage(Target::Server, false);
	}
	//  If from server, send current set of clients
	else
	{
		Message::Packet packet;
		packet.source.author = configuration.name;
		packet.payload.emplace<Message::PeerInformationSet>(getPeers());
		return connection.sendToPeers(std::move(packet), Target::Client, false);
	}
}
bool ServerConnection::MessageHandler::operator()(Message::StreamUpdate &su)
{
	CHECK_INFO(Message::StreamUpdate)

	const auto &info = connection.getInfo();
	switch (su.action)
	{
	case Message::StreamUpdate::Create: {
		if (!info.isServer)
		{
			// If client tries to create stream, ensure they are the author
			if (su.source.author != info.name)
			{
				PEER_ERROR("Failed to create stream. Author doesn't match");
			}
		}
		// Ensure client hasn't created too many streams
		if (streams.size() >= configuration.maxStreamsPerClient)
		{
			(*logger)(Logger::Warning) << info << " tried to make more streams than allowed ("
									   << configuration.maxStreamsPerClient << ")" << std::endl;
			return sendStreamsToClients();
		}
		if (totalStreams() >= configuration.maxTotalStreams)
		{
			(*logger)(Logger::Warning) << info << " tried to make a stream when the server max has been reached ("
									   << configuration.maxTotalStreams << ")" << std::endl;
			return sendStreamsToClients();
		}
		LOCK(peersMutex);
		auto result = ServerConnection::streams.try_emplace(Message::Source(su.source), Stream(su.source, su.type));
		if (result.second)
		{
			*logger << "Stream created: " << result.first->first << '(' << result.first->second.getType() << ')'
					<< std::endl;
			if (!sendStreamsToClients())
			{
				return false;
			}
		}
		else
		{
			(*logger)(Logger::Warning) << "Stream already exists: " << su.source << std::endl;
		}
	}
	break;
	case Message::StreamUpdate::Delete: {
		LOCK(peersMutex);
		if (!info.isServer)
		{
			// If client tries to delete stream, ensure they are the author
			if (su.source.author != info.name)
			{
				PEER_ERROR("Author doesn't match");
			}
		}

		auto result = ServerConnection::streams.erase(su.source);
		if (result > 0)
		{
			*logger << info << " deleted stream: " << su.source << std::endl;
			if (!sendStreamsToClients())
			{
				return false;
			}
		}
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
		// Ensure stream exists
		if (!ServerConnection::getStream(su.source).has_value())
		{
			*logger << info << " tried to subscribe to non-existing stream " << su.source << std::endl;
			break;
		}
		connection.subscriptions.emplace(su.source);
		*logger << info << " subscribed stream " << su.source << std::endl;

		// Send any potential stored data the stream might have
		if (!sendPayloadForStream(su.source))
		{
			return false;
		}
		// Don't send message to other servers
		return sendSubscriptionsToClient();
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
		*logger << info << " unsubscribed stream " << su.source << std::endl;
		// Don't send message to other servers
		return sendSubscriptionsToClient();
	}
	break;
	default:
		(*logger)(Logger::Error) << "Invalid action " << su.action << std::endl;
		return false;
	}
	return processCurrentMessage(Target::Server, false);
}
bool ServerConnection::MessageHandler::operator()(Message::GetStreams &)
{
	CHECK_INFO(Message::GetStreams)
	// Send get streams to servers
	if (!processCurrentMessage(Target::Server, false))
	{
		return false;
	}

	// Send current streams to all clients
	return sendStreamsToClients();
}
bool ServerConnection::MessageHandler::operator()(Message::Streams &streams)
{
	CHECK_INFO(Message::Streams)
	LOCK(peersMutex);
	// Server only. Append to the list of streams and send to clients
	if (!connection.isServer())
	{
		BAD_MESSAGE(Streams);
	}
	ServerConnection::streams.insert(streams.begin(), streams.end());
	return sendStreamsToClients();
}
bool ServerConnection::MessageHandler::sendStreamsToClients() const
{
	LOCK(peersMutex);
	Message::Packet packet;
	packet.source.author = ServerConnection::configuration.name;
	packet.payload.emplace<Message::Streams>(ServerConnection::streams);
	(*logger)(Logger::Trace) << "Sending " << ServerConnection::streams.size()
							 << " streams to peers: " << connection.getInfo() << std::endl;
	return ServerConnection::sendToPeers(std::move(packet), Target::Client, false);
}
bool ServerConnection::MessageHandler::operator()(Message::GetSubscriptions &)
{
	// Send get subscription to servers
	if (!processCurrentMessage(Target::Server, false))
	{
		return false;
	}

	// Send current subscriptions to clients
	return sendSubscriptionsToClient();
}
bool ServerConnection::MessageHandler::operator()(Message::Subscriptions &subs)
{
	CHECK_INFO(Message::Subscriptions)
	// Server only. Send current subscriptions to clients
	if (!connection.isServer())
	{
		BAD_MESSAGE(Subscriptions);
	}
	connection.subscriptions.insert(subs.begin(), subs.end());
	return sendSubscriptionsToClient();
}
bool ServerConnection::MessageHandler::sendSubscriptionsToClient() const
{
	(*logger)(Logger::Trace) << "Sending " << connection.subscriptions.size()
							 << " subscriptions to peer: " << connection.getInfo() << std::endl;
	Message::Packet packet;
	packet.source.author = ServerConnection::configuration.name;
	packet.payload.emplace<Message::Subscriptions>(connection.subscriptions);
	return connection->sendPacket(packet);
}
bool ServerConnection::MessageHandler::savePayloadIfNedded(bool append) const
{
	if (!packet.trail.empty() || packet.source.author != connection.info.name)
	{
		return false;
	}

	auto stream = ServerConnection::getStream(packet.source);
	if (!stream.has_value())
	{
		return false;
	}

	std::array<char, KB(1)> buffer;
	stream->getFileName(buffer);
	auto flags = std::ios::out;
	if (append)
	{
		flags |= std::ios::app;
	}
	else
	{
		flags |= std::ios::trunc;
	}
	std::ofstream file(buffer.data(), flags);
	if (!file.is_open())
	{
		return false;
	}

	try
	{
		cereal::PortableBinaryOutputArchive ar(file);
		ar(packet.payload);

		return true;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Failed to save payload for stream " << *stream << ": " << e.what() << std::endl;
		return false;
	}
}
bool ServerConnection::MessageHandler::sendPayloadForStream(const Message::Source &source)
{
	auto stream = ServerConnection::getStream(source);
	if (!stream.has_value())
	{
		return true;
	}

	std::array<char, KB(1)> buffer;
	stream->getFileName(buffer);

	switch (stream->getType())
	{
	case variant_index<Message::Payload, Message::Text>():
		try
		{
			Message::Payload payload;
			{
				std::ifstream file(buffer.data());
				if (!file.is_open())
				{
					return true;
				}
				cereal::PortableBinaryInputArchive ar(file);
				ar(payload);
			}
			Message::Packet packet;
			packet.source = stream->getSource();
			packet.payload = std::move(payload);
			if (!connection->sendPacket(packet))
			{
				return false;
			}
		}
		catch (const std::exception &e)
		{
			(*logger)(Logger::Error) << "Failed to load payload for stream " << *stream << ": " << e.what()
									 << std::endl;
			return false;
		}
		break;
	case variant_index<Message::Payload, Message::Image>(): {
		std::thread thread(MessageHandler::sendImageBytes, connection.getPointer(),
						   Message::Source(stream->getSource()), String(buffer.data()));
		thread.detach();
	}
	break;
	default:
		break;
	}
	return true;
}
void ServerConnection::MessageHandler::sendImageBytes(shared_ptr<ServerConnection> ptr, Message::Source &&source,
													  String &&filename)
{

	std::ifstream file(filename.c_str(), std::ios::binary | std::ios::out);
	if (!file.is_open())
	{
		return;
	}

	Message::prepareImageBytes(file, source, [ptr](Message::Packet &&packet) { (*ptr)->sendPacket(packet); });
}
ServerConnection::ImageSaver::ImageSaver(ServerConnection &connection, const Message::Source &source)
	: connection(connection), source(source)
{
}
ServerConnection::ImageSaver::~ImageSaver()
{
}
void ServerConnection::ImageSaver::operator()(uint64_t)
{
	auto stream = ServerConnection::getStream(source);
	if (!stream.has_value())
	{
		return;
	}

	std::array<char, KB(1)> buffer;
	stream->getFileName(buffer);
	std::filesystem::remove(buffer.data());
}
void ServerConnection::ImageSaver::operator()(const ByteList &bytes)
{
	auto stream = ServerConnection::getStream(source);
	if (!stream.has_value())
	{
		return;
	}

	std::array<char, KB(1)> buffer;
	stream->getFileName(buffer);
	std::ofstream file(buffer.data(), std::ios::app | std::ios::out | std::ios::binary);
	if (!file.is_open())
	{
		return;
	}

	std::ostreambuf_iterator<char> iter(file);
	std::copy(bytes.begin(), bytes.end(), iter);
}
void ServerConnection::ImageSaver::operator()(std::monostate)
{
}
ServerConnection::CredentialHandler::CredentialHandler(VerifyToken verifyToken,
													   VerifyUsernameAndPassword verifyUsernameAndPassword)
	: verifyToken(verifyToken), verifyUsernameAndPassword(verifyUsernameAndPassword)
{
}
ServerConnection::CredentialHandler::~CredentialHandler()
{
}
std::optional<PeerInformation> ServerConnection::CredentialHandler::operator()(String &&token)
{
	PeerInformation info;
	if (verifyToken)
	{
		char username[KB(1)];
		if (verifyToken(token.c_str(), username, &info.isServer))
		{
			info.name = username;
		}
		else
		{
			return std::nullopt;
		}
	}
	else
	{
		// Default credentials always set peer to client
		info.name = std::move(token);
		info.isServer = false;
	}
	return info;
}
std::optional<PeerInformation> ServerConnection::CredentialHandler::operator()(Message::UsernameAndPassword &&pair)
{
	PeerInformation info;
	if (verifyUsernameAndPassword)
	{
		char username[KB(1)];
		if (verifyUsernameAndPassword(pair.first.c_str(), pair.second.c_str(), username, &info.isServer))
		{
			info.name = username;
		}
		else
		{
			return std::nullopt;
		}
	}
	else
	{
		// Default credentials always set peer to client
		info.name = std::move(pair.first);
		info.isServer = false;
	}
	return info;
}
} // namespace TemStream