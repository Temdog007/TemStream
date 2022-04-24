#include <main.hpp>

namespace TemStream
{
ClientConnetion::ClientConnetion(const Address &address, unique_ptr<Socket> s)
	: Connection(address, std::move(s)), acquiredServerInformation(false)
{
}
ClientConnetion::~ClientConnetion()
{
}
bool ClientConnetion::handlePacket(Message::Packet &&packet)
{
	auto ptr = std::get_if<PeerInformation>(&packet.payload);
	if (ptr == nullptr)
	{
		addPacket(std::move(packet));
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
	auto packet = allocate<Message::Packet>(std::move(m));
	e.user.data1 = packet;
	e.user.data2 = nullptr;
	if (!tryPushEvent(e))
	{
		deallocate(packet);
	}
}
void ClientConnetion::addPackets(MessagePackets &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePackets;
	auto packets = allocate<MessagePackets>(std::move(m));
	e.user.data1 = packets;
	e.user.data2 = nullptr;
	if (!tryPushEvent(e))
	{
		deallocate(packets);
	}
}
} // namespace TemStream