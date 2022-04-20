#include <main.hpp>

namespace TemStream
{
ClientPeer::ClientPeer(const Address &address, unique_ptr<Socket> s)
	: Peer(address, std::move(s)), acquiredServerInformation(false)
{
}
ClientPeer::~ClientPeer()
{
}
bool ClientPeer::handlePacket(MessagePacket &&packet)
{
	auto ptr = std::get_if<PeerInformation>(&packet.message);
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
void ClientPeer::addPacket(MessagePacket &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePacket;
	auto packet = allocate<MessagePacket>(std::move(m));
	e.user.data1 = packet;
	if (!tryPushEvent(e))
	{
		deallocate(packet);
	}
}
void ClientPeer::addPackets(MessagePackets &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePackets;
	auto packets = allocate<MessagePackets>(std::move(m));
	e.user.data1 = packets;
	if (!tryPushEvent(e))
	{
		deallocate(packets);
	}
}
} // namespace TemStream