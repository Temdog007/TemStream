#include <main.hpp>

namespace TemStream
{
ClientConnection::ClientConnection(TemStreamGui &gui, const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), gui(gui), verifyLogin(), serverInformation(), lastSentMessage(), opened(true)
{
}
ClientConnection::~ClientConnection()
{
}
void ClientConnection::close()
{
	if (!isOpened())
	{
		return;
	}
	opened = false;
	(*logger)(Logger::Info) << "Closing connection: " << getSource() << std::endl;
}
bool ClientConnection::sendPacket(const Message::Packet &packet, const bool sendImmediately)
{
	lastSentMessage = std::chrono::system_clock::now();
	return mSocket->sendPacket(packet, sendImmediately);
}
bool ClientConnection::flushPackets()
{
	using namespace std::chrono_literals;
	auto packet = getPackets().pop(0s);
	if (!packet)
	{
		return true;
	}

	// Send audio data to playback immediately to avoid audio delay
	if (auto message = std::get_if<Message::Audio>(&packet->payload))
	{
		gui.useAudio(packet->source, [&message](AudioSource &a) {
			if (!a.isRecording())
			{
				a.enqueueAudio(message->bytes);
			}
		});
	}
	addPacket(std::move(*packet));
	return true;
}
void ClientConnection::addPacket(Message::Packet &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePacket;
	auto packet = allocateAndConstruct<Message::Packet>(std::move(m));
	e.user.data1 = packet;
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(packet);
	}
}
void ClientConnection::addPackets(MessagePackets &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePackets;
	auto packets = allocateAndConstruct<MessagePackets>(std::move(m));
	e.user.data1 = packets;
	e.user.data2 = nullptr;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(packets);
	}
}
Message::Source ClientConnection::getSource() const
{
	Message::Source source;
	source.serverName = verifyLogin.serverName;
	source.address = address;
	return source;
}
std::optional<std::chrono::duration<double>> ClientConnection::nextSendInterval() const
{
	if (verifyLogin.sendRate == 0)
	{
		return std::nullopt;
	}

	const auto now = std::chrono::system_clock::now();
	const auto diff = lastSentMessage + std::chrono::duration<uint32_t>(verifyLogin.sendRate);
	if (now < diff)
	{
		return diff - now;
	}
	return std::nullopt;
}
} // namespace TemStream