#include <main.hpp>

#include "badWords.hpp"

#define CHECK_INFO(X)                                                                                                  \
	if (connection.information.name.empty())                                                                           \
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
StringList badWords;

int runApp(Configuration &configuration)
{
	logger = tem_unique<ConsoleLogger>();
	TemStream::initialLogs();

	*logger << configuration << std::endl;

	int result = EXIT_FAILURE;
	int fd = -1;

	{
		String s(reinterpret_cast<char *>(List_of_Dirty_Naughty_Obscene_and_Otherwise_Bad_Words_en),
				 List_of_Dirty_Naughty_Obscene_and_Otherwise_Bad_Words_en_len);
		StringStream ss(s);
		String to;
		while (std::getline(ss, to))
		{
			badWords.emplace_back(std::move(to));
		}
	}

	ServerConnection::configuration = configuration;

	if (configuration.serverType == ServerType::Link)
	{
		std::thread thread([]() {
			String filename = ServerConnection::configuration.name;
			filename += ".json";
			ServerConnection::sendLinks(filename);
			auto lastWrite = std::filesystem::last_write_time(filename);
			using namespace std::chrono_literals;
			while (!appDone)
			{
				const auto now = std::filesystem::last_write_time(filename);
				if (now != lastWrite)
				{
					ServerConnection::sendLinks(filename);
					lastWrite = now;
				}
				std::this_thread::sleep_for(1s);
			}
		});
		thread.detach();
	}

	if (!openSocket(fd, configuration.address, true, true))
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

			auto peer = tem_shared<ServerConnection>(Address(str.data(), port), std::move(s));
			++ServerConnection::runningThreads;
			std::thread thread(ServerConnection::runPeerConnection, std::move(peer));
			thread.detach();
		}
	}

	result = EXIT_SUCCESS;

end:
	::close(fd);
	*logger << "Ending server: " << configuration.name << std::endl;
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

	using namespace std::chrono_literals;
	while (!appDone && stayConnected)
	{
		try
		{
			if (!readAndHandle(1000))
			{
				break;
			}
			if (!isAuthenticated() && std::chrono::system_clock::now() - startingTime > 10s)
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
void ServerConnection::sendToPeers(Message::Packet &&packet, const ServerConnection *author)
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
			// Don't send packet to peer author or if the peer isn't authenticated
			if (ptr.get() != author && ptr->isAuthenticated())
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
List<PeerInformation> ServerConnection::getPeers()
{
	List<PeerInformation> list;
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (auto ptr = iter->lock())
		{
			list.push_back(ptr->information);
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
	return list;
}
void ServerConnection::checkAccess()
{
	LOCK(peersMutex);
	for (auto iter = peers.begin(); iter != peers.end();)
	{
		if (auto ptr = iter->lock())
		{
			if (configuration.access.isBanned(ptr->information.name))
			{
				ptr->stayConnected = false;
			}
			++iter;
		}
		else
		{
			iter = peers.erase(iter);
		}
	}
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
void ServerConnection::sendLinks()
{
	String filename = ServerConnection::configuration.name;
	filename += ".json";
	return sendLinks(filename);
}
void ServerConnection::sendLinks(const String &filename)
{
	try
	{
		// Required to use STL containers for JSON serializing
		std::vector<Message::BaseServerLink<std::string>> temp;
		{
			std::ifstream file(filename.c_str());
			cereal::JSONInputArchive ar(file);
			ar(cereal::make_nvp("servers", temp));
		}
		Message::ServerLinks links;
		auto pair = toMoveIterator(std::move(temp));
		std::transform(pair.first, pair.second, std::inserter(links, links.begin()),
					   [](Message::BaseServerLink<std::string> &&s) {
						   Message::ServerLink link(std::move(s));
						   return link;
					   });

		(*logger)(Logger::Trace) << "Sending links: " << links.size() << std::endl;
		Message::Packet packet;
		packet.source = ServerConnection::getSource();
		packet.payload.emplace<Message::ServerLinks>(std::move(links));
		ServerConnection::sendToPeers(std::move(packet));
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << e.what() << std::endl;
	}
}
ServerConnection::ServerConnection(Address &&address, unique_ptr<Socket> s)
	: Connection(std::move(address), std::move(s)), startingTime(std::chrono::system_clock::now()), stayConnected(true)
{
}
ServerConnection::~ServerConnection()
{
}
bool ServerConnection::isAuthenticated() const
{
	return !information.name.empty();
}
Message::Source ServerConnection::getSource()
{
	Message::Source source;
	source.address = ServerConnection::configuration.address;
	source.serverName = ServerConnection::configuration.name;
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
	if (packet.payload.index() != ServerTypeToIndex(configuration.serverType))
	{
		(*logger)(Logger::Error) << "Server got invalid message type: " << packet.payload.index() << std::endl;
		return false;
	}
	if (!connection.information.hasWriteAccess())
	{
		(*logger)(Logger::Error) << "Peer doesn't have write access: " << connection.information << std::endl;
		return false;
	}
	const auto now = std::chrono::system_clock::now();
	if (configuration.messageRateInSeconds != 0)
	{
		const auto timepoint =
			connection.lastMessage + std::chrono::duration<uint32_t>(configuration.messageRateInSeconds);
		if (now < timepoint)
		{
			(*logger)(Logger::Error) << "Peer is sending packets too frequently: " << connection.information
									 << std::endl;
			return false;
		}
	}
	connection.lastMessage = now;
	ServerConnection::sendToPeers(std::move(packet), &connection);
	return true;
}
bool ServerConnection::MessageHandler::operator()()
{
	if (!std::holds_alternative<Message::Credentials>(packet.payload) && packet.source != ServerConnection::getSource())
	{
		(*logger)(Logger::Error) << "Server got message with wrong server address: " << packet.source << std::endl;
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
bool ServerConnection::MessageHandler::operator()(Message::Chat &chat)
{
	CHECK_INFO(Message::Chat)
	// Check for profainity
	auto &message = chat.message;
	for (const auto &word : badWords)
	{
		while (true)
		{
			auto iter = std::search(message.begin(), message.end(), word.begin(), word.end(),
									[](char c1, char c2) { return std::toupper(c1) == std::toupper(c2); });
			if (iter == message.end())
			{
				break;
			}

			std::replace_if(
				iter, iter + word.size(), [](auto) { return true; }, '*');
		}
	}
	chat.timestamp = static_cast<int64_t>(time(nullptr));
	chat.author = connection.information.name;
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
std::optional<PeerInformation> ServerConnection::login(const Message::Credentials &credentials)
{
	return std::visit(CredentialHandler(ServerConnection::configuration), credentials);
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
	checkAccess();
	if (!connection.stayConnected)
	{
		(*logger)(Logger::Warning) << "Peer " << connection.information << "  is banned" << std::endl;
		return false;
	}
	*logger << "Peer: " << connection.address << " -> " << connection.information << std::endl;
	{
		Message::Packet packet;
		packet.source = ServerConnection::getSource();
		Message::VerifyLogin login;
		login.serverName = configuration.name;
		login.sendRate = configuration.messageRateInSeconds;
		login.serverType = configuration.serverType;
		login.peerInformation = connection.information;
		packet.payload.emplace<Message::VerifyLogin>(std::move(login));
		connection->sendPacket(packet);
	}

	return sendStoredPayload();
}
bool ServerConnection::MessageHandler::operator()(Message::BanUser &banUser)
{
	if (!connection.information.isModerator())
	{
		(*logger)(Logger::Error) << "Non-moderator peer " << connection.information << " tried to change ban a user"
								 << std::endl;
		return false;
	}
	if (!ServerConnection::configuration.access.banList)
	{
		(*logger)(Logger::Error) << "Peer " << connection.information
								 << " tried to change ban a user when there is no ban list" << std::endl;
		return false;
	}
	{
		LOCK(peersMutex);
		for (auto iter = ServerConnection::peers.begin(); iter != ServerConnection::peers.end();)
		{
			if (auto ptr = iter->lock())
			{
				if (ptr->information.name == banUser.name)
				{
					if (ptr->information.isModerator())
					{
						(*logger)(Logger::Warning) << "Moderator " << connection.information
												   << " tried to ban user: " << ptr->information << std::endl;
					}
					else
					{
						*logger << "Banned user: " << ptr->information << std::endl;
						ServerConnection::configuration.access.members.insert(ptr->information.name);
					}
					break;
				}
				++iter;
			}
			else
			{
				iter = ServerConnection::peers.erase(iter);
			}
		}
	}
	ServerConnection::checkAccess();
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
bool ServerConnection::MessageHandler::operator()(Message::ServerInformation &)
{
	BAD_MESSAGE(ServerInformation);
}
bool ServerConnection::MessageHandler::operator()(Message::RequestServerInformation &)
{
	CHECK_INFO(Message::RequestServerInformation)
	if (!connection.information.isModerator())
	{
		(*logger)(Logger::Error) << "Non-moderator peer " << connection.information << " tried to get peer list";
		return false;
	}
	Message::Packet packet;
	packet.source = ServerConnection::getSource();
	Message::ServerInformation info;
	info.peers = getPeers();
	if (ServerConnection::configuration.access.banList)
	{
		info.banList = ServerConnection::configuration.access.members;
	}
	packet.payload.emplace<Message::ServerInformation>(std::move(info));
	connection->sendPacket(packet);
	return true;
}
bool ServerConnection::MessageHandler::savePayloadIfNedded(bool append) const
{
	if (packet.source != ServerConnection::getSource())
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
	case ServerType::Link:
		ServerConnection::sendLinks();
		return true;
	case ServerType::Text:
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
	case ServerType::Image: {
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
CredentialHandler::CredentialHandler(VerifyToken verifyToken, VerifyUsernameAndPassword verifyUsernameAndPassword)
	: verifyToken(verifyToken), verifyUsernameAndPassword(verifyUsernameAndPassword)
{
}
CredentialHandler::CredentialHandler(const Configuration &configuration)
	: verifyToken(configuration.verifyToken), verifyUsernameAndPassword(configuration.verifyUsernameAndPassword)
{
}
CredentialHandler::~CredentialHandler()
{
}
std::optional<PeerInformation> CredentialHandler::operator()(const String &token)
{
	PeerInformation info;
	if (verifyToken)
	{
		char username[32];
		uint32_t flags = 0;
		if (verifyToken(token.c_str(), username, &flags))
		{
			info.name = username;
			info.flags = static_cast<PeerFlags>(flags);
		}
		else
		{
			return std::nullopt;
		}
	}
	else
	{
		// Default credentials always sets the owner flag
		// Don't use in production!!!
		info.name = std::move(token);
		info.flags = PeerFlags::Owner;
	}
	return info;
}
std::optional<PeerInformation> CredentialHandler::operator()(const Message::UsernameAndPassword &pair)
{
	PeerInformation info;
	if (verifyUsernameAndPassword)
	{
		uint32_t flags = 0;
		if (verifyUsernameAndPassword(pair.first.c_str(), pair.second.c_str(), &flags))
		{
			info.name = pair.first;
			info.flags = static_cast<PeerFlags>(flags);
		}
		else
		{
			return std::nullopt;
		}
	}
	else
	{
		// Default credentials always sets the owner flag
		// Don't use in production!!!
		info.name = std::move(pair.first);
		info.flags = PeerFlags::Owner;
	}
	return info;
}
} // namespace TemStream