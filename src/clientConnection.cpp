#include <main.hpp>

namespace TemStream
{
ClientConnection::ClientConnection(TemStreamGui &gui, const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), gui(gui), serverInformation(), peers()
{
}
ClientConnection::~ClientConnection()
{
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
	e.user.data2 = nullptr;
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
	source.server = serverInformation.serverName;
	source.peer = serverInformation.peerInformation.name;
	return source;
}
} // namespace TemStream