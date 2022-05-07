#include "chatTester.hpp"

namespace TemStream
{
void runConnection(const Message::Source &);
String randomString(size_t, size_t);
Message::Chat randomChatMessage(const String &);

int runApp(Configuration &configuration)
{
	logger = tem_unique<ConsoleLogger>();
	initialLogs();

	List<std::thread> threads;
	const Message::Source source{Address(configuration.hostname, configuration.port), configuration.serverName};
	for (int i = 0; i < configuration.senders; ++i)
	{
		threads.emplace_back(&runConnection, source);
	}
	for (auto &thread : threads)
	{
		thread.join();
	}
	return EXIT_SUCCESS;
}

void runConnection(const Message::Source &source)
{
	auto socket = TcpSocket::create(source.address);
	if (socket == nullptr)
	{
		std::cerr << "Failed to connect to server" << std::endl;
		return;
	}

	Connection connection(source.address, std::move(socket));
	auto &packets = connection.getPackets();

	Message::Packet packet;
	packet.source = source;
	packet.payload.emplace<Message::Credentials>(randomString(3, 10));
	if (!connection->sendPacket(packet, true))
	{
		std::cerr << "Failed to send to server" << std::endl;
		return;
	}

	Message::VerifyLogin info;
	using namespace std::chrono_literals;
	while (!appDone && connection.readAndHandle(3000))
	{
		auto temp = packets.pop(0s);
		if (temp)
		{
			if (auto p = std::get_if<Message::VerifyLogin>(&temp->payload))
			{
				info = *p;
				break;
			}
			else
			{
				std::cerr << "Invalid message" << std::endl;
				return;
			}
		}
	}

	std::cout << "Logged in as " << info.peerInformation << std::endl;

	while (!appDone)
	{
		packet.payload.emplace<Message::Chat>(randomChatMessage(info.peerInformation.name));
		if (!connection->sendPacket(packet, true) && !connection.readAndHandle(0))
		{
			break;
		}
		packets.clear();
		const int seconds = info.sendRate + std::rand() % 3;
		std::this_thread::sleep_until(std::chrono::system_clock::now() + std::chrono::seconds(seconds));
	}

	std::cout << "Ending connection for " << info.peerInformation << std::endl;
}

String randomString(size_t min, size_t max)
{
	String s;
	const size_t n = min + static_cast<size_t>(roundf((max - min) * static_cast<float>(rand()) / RAND_MAX));
	while (s.size() < n)
	{
		const char c = rand() % 128;
		if (isprint(c))
		{
			s += c;
		}
	}
	return s;
}

Message::Chat randomChatMessage(const String &author)
{
	Message::Chat chat;
	chat.author = author;
	chat.message = randomString(10, 128);
	return chat;
}

Configuration loadConfiguration(int argc, const char *argv[])
{
	Configuration configuration;
	if (argc < 5)
	{
		throw std::runtime_error("Need args: hostname, port, server name, and sender");
	}
	configuration.hostname = argv[1];
	configuration.port = atoi(argv[2]);
	configuration.serverName = argv[3];
	configuration.senders = atoi(argv[4]);
	return configuration;
}

void saveConfiguration(const Configuration &)
{
}
} // namespace TemStream