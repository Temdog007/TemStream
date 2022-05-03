#include <main.hpp>

#define CHECK_INFO(X)                                                                                                  \
	if (!connection.information.name.empty())                                                                          \
	{                                                                                                                  \
		logger->AddError("Got " #X " from peer before getting their information");                                     \
		return false;                                                                                                  \
	} // namespace TemStream
#define BAD_MESSAGE(X)                                                                                                 \
	(*logger)(Logger::Error) << "Client " << connection.information.name << " sent invalid message: '" #X "'"          \
							 << std::endl;                                                                             \
	return false

namespace TemStream
{
std::atomic_int32_t ServerConnection::runningThreads = 0;
Configuration ServerConnection::configuration;
Mutex ServerConnection::peersMutex;
LinkedList<std::weak_ptr<ServerConnection>> ServerConnection::peers;

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
	*logger << "Ending server: " << configuration.name;
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

	std::thread thread(&ServerConnection::handleOutput, this);
	std::thread sender([this]() {
		using namespace std::chrono_literals;
		while (!appDone && this->stayConnected)
		{
			stayConnected = (*this)->flush();
			std::this_thread::sleep_for(1ms);
		}
	});

	while (!appDone && stayConnected)
	{
		try
		{
			if (!readAndHandle(1000))
			{
				break;
			}
		}
		catch (const std::bad_alloc &)
		{
			(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
		}
		catch (const std::exception &e)
		{
			(*logger)(Logger::Error) << "Exception occurred: " << e.what() << std::endl;
		}
	}

	stayConnected = false;
	thread.join();
	sender.join();
}
void ServerConnection::handleOutput()
{
	using namespace std::chrono_literals;
	auto &packets = getPackets();
	while (!appDone && stayConnected)
	{
		auto packet = packets.pop(100ms);
		if (!packet)
		{
			continue;
		}
		try
		{
			stayConnected = ServerConnection::MessageHandler(*this, std::move(*packet))();
		}
		catch (const std::bad_alloc &)
		{
			(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
		}
		catch (const std::exception &e)
		{
			(*logger)(Logger::Error) << "Exception occurred: " << e.what() << std::endl;
		}
	}
}
void ServerConnection::sendToPeers(Message::Packet &&packet)
{
	MemoryStream m;
	{
		cereal::PortableBinaryOutputArchive ar(m);
		ar(packet);
	}

	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerConnection> ptr = iter->lock())
		{
			// Don't send packet to peer
			if ((*ptr).information.name != packet.source.peer)
			{
				(*ptr)->send(m->getData(), m->getSize());
			}
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
}

bool ServerConnection::peerExists(const String &name)
{
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (shared_ptr<ServerConnection> ptr = iter->lock())
		{
			if ((*ptr).information.name == name)
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
size_t ServerConnection::totalPeers()
{
	LOCK(peersMutex);
	return ServerConnection::peers.size();
}
StringList ServerConnection::getPeers()
{
	StringList list;
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (auto ptr = iter->lock())
		{
			list.push_back(ptr->information.name);
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
	return list;
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
	: Connection(address, std::move(s)), stayConnected(true)
{
}
ServerConnection::~ServerConnection()
{
}
Message::Source ServerConnection::getSource() const
{
	Message::Source source;
	source.peer = information.name;
	source.server = configuration.name;
	return source;
}
ServerConnection::MessageHandler::MessageHandler(ServerConnection &connection, Message::Packet &&packet)
	: connection(connection), packet(std::move(packet))
{
}
ServerConnection::MessageHandler::~MessageHandler()
{
}
bool ServerConnection::MessageHandler::processCurrentMessage()
{
	if (packet.payload.index() != configuration.serverType)
	{
		return false;
	}
	ServerConnection::sendToPeers(std::move(packet));
	return true;
}
bool ServerConnection::MessageHandler::operator()()
{
	if (packet.source.server != configuration.name)
	{
		return false;
	}
	return std::visit(*this, packet.payload);
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
	std::visit(ImageSaver(connection, packet.source), image.largeFile);
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
std::optional<Message::PeerInformation> ServerConnection::login(const Message::Credentials &credentials)
{
	// TODO: Load dll to handle credentials
	return std::visit(CredentialHandler(nullptr, nullptr), std::move(credentials));
}
bool ServerConnection::MessageHandler::operator()(Message::Credentials &credentials)
{
	if (!connection.information.name.empty())
	{
		(*logger)(Logger::Error) << "Peer sent credentials more than once" << std::endl;
		return false;
	}
	auto info = ServerConnection::login(credentials);
	if (!info.has_value() || info->name.empty())
	{
		(*logger)(Logger::Error) << "Invalid credentials sent" << std::endl;
		return false;
	}
	if (ServerConnection::peerExists(info->name))
	{
		(*logger)(Logger::Error) << "Duplicate peer " << *info << " attempted to connect" << std::endl;
		return false;
	}
	connection.information.swap(*info);
	*logger << "Peer: " << connection.address << " -> " << connection.information << std::endl;
	{
		Message::Packet packet;
		packet.source = connection.getSource();
		packet.payload.emplace<Message::VerifyLogin>(
			Message::VerifyLogin{configuration.name, connection.information, configuration.serverType});
		connection->sendPacket(packet);
	}
	return true;
}
bool ServerConnection::MessageHandler::operator()(Message::ServerLinks &)
{
	BAD_MESSAGE(ServerLinks);
}
bool ServerConnection::MessageHandler::operator()(Message::VerifyLogin &)
{
	BAD_MESSAGE(VerifyLogin);
}
bool ServerConnection::MessageHandler::operator()(Message::RequestPeers &)
{
	CHECK_INFO(Message::RequestPeers)
	LOCK(peersMutex);
	Message::Packet packet;
	packet.source = connection.getSource();
	packet.payload.emplace<Message::PeerList>(Message::PeerList{getPeers()});
	connection->sendPacket(packet);
	return true;
}
bool ServerConnection::MessageHandler::operator()(Message::PeerList &)
{
	BAD_MESSAGE(PeerList);
}
bool ServerConnection::MessageHandler::savePayloadIfNedded(bool append) const
{
	if (packet.source.peer != connection.information.name)
	{
		return false;
	}

	std::array<char, KB(1)> buffer;
	ServerConnection::getFilename(buffer);
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
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Failed to save payload for stream " << configuration.name << ": " << e.what()
								 << std::endl;
	}
	return false;
}
bool ServerConnection::MessageHandler::sendStoredPayload()
{
	std::array<char, KB(1)> buffer;
	ServerConnection::getFilename(buffer);

	switch (configuration.serverType)
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
			packet.source = connection.getSource();
			packet.payload = std::move(payload);
			connection->sendPacket(packet);
		}
		catch (const std::exception &e)
		{
			(*logger)(Logger::Error) << "Failed to load payload for server: " << e.what() << std::endl;
			return false;
		}
		break;
	case variant_index<Message::Payload, Message::Image>(): {
		std::thread thread(MessageHandler::sendImageBytes, connection.getPointer(), connection.getSource(),
						   String(buffer.data()));
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

	Message::prepareLargeBytes(file, [ptr, &source](Message::LargeFile &&lf) {
		Message::Packet packet;
		packet.source = source;
		Message::Image image{std::move(lf)};
		packet.payload.emplace<Message::Image>(std::move(image));
		(*ptr)->sendPacket(packet);
	});
}
ServerConnection::ImageSaver::ImageSaver(ServerConnection &connection, const Message::Source &source)
	: connection(connection), source(source)
{
}
ServerConnection::ImageSaver::~ImageSaver()
{
}
void ServerConnection::ImageSaver::operator()(const Message::LargeFile &lf)
{
	std::visit(*this, lf);
}
void ServerConnection::ImageSaver::operator()(uint64_t)
{
	std::array<char, KB(1)> buffer;
	ServerConnection::getFilename(buffer);
	std::filesystem::remove(buffer.data());
}
void ServerConnection::ImageSaver::operator()(const ByteList &bytes)
{
	std::array<char, KB(1)> buffer;
	ServerConnection::getFilename(buffer);
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
std::optional<Message::PeerInformation> ServerConnection::CredentialHandler::operator()(const String &token)
{
	Message::PeerInformation info;
	if (verifyToken)
	{
		char username[KB(1)];
		if (verifyToken(token.c_str(), username, &info.writeAccess))
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
		// Default credentials always give peer write access
		// Don't use in production!!!
		info.name = std::move(token);
		info.writeAccess = true;
	}
	return info;
}
std::optional<Message::PeerInformation> ServerConnection::CredentialHandler::operator()(
	const Message::UsernameAndPassword &pair)
{
	Message::PeerInformation info;
	if (verifyUsernameAndPassword)
	{
		char username[KB(1)];
		if (verifyUsernameAndPassword(pair.first.c_str(), pair.second.c_str(), username, &info.writeAccess))
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
		// Default credentials always give peer write access
		// Don't use in production!!!
		info.name = std::move(pair.first);
		info.writeAccess = false;
	}
	return info;
}
} // namespace TemStream