#include <main.hpp>

namespace TemStream
{
ClientConnetion::ClientConnetion(TemStreamGui &gui, const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), gui(gui), acquiredServerInformation(false)
{
}
ClientConnetion::~ClientConnetion()
{
}
bool ClientConnetion::flushPackets()
{
	using namespace std::chrono_literals;
	auto packet = getPackets().pop(0s);
	if (!packet)
	{
		return true;
	}
	auto ptr = std::get_if<PeerInformation>(&packet->payload);
	if (ptr == nullptr)
	{
		// Send audio data to playback immediately to avoid audio issues
		if (auto message = std::get_if<Message::Audio>(&packet->payload))
		{
			gui.useAudio(packet->source, [&message](Audio &a) {
				if (!a.isRecording())
				{
					a.enqueueAudio(message->bytes);
				}
			});
		}
		addPacket(std::move(*packet));
	}
	else if (acquiredServerInformation)
	{
		logger->AddError("Got duplicate information from server");
		return false;
	}
	else
	{
		if (ptr->isServer)
		{
			info = *ptr;
			acquiredServerInformation = true;
		}
		else
		{
			logger->AddError("Connection error. Must connect to a server. Connected to a client");
			return false;
		}
	}
	return true;
}
void ClientConnetion::addPacket(Message::Packet &&m)
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
void ClientConnetion::addPackets(MessagePackets &&m)
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
} // namespace TemStream