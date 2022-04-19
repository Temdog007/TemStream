#include <main.hpp>

namespace TemStream
{
ClientPeer::ClientPeer(const Address &address, std::unique_ptr<Socket> s)
	: Peer(address, std::move(s)), gotInformation(false)
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
	else if (gotInformation)
	{
		logger->AddError("Got duplicate information from server");
		return false;
	}
	else
	{
		if (ptr->isServer)
		{
			info = *ptr;
			gotInformation = true;
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
	auto packet = new MessagePacket(std::move(m));
	e.user.data1 = packet;
	if (!tryPushEvent(e))
	{
		delete packet;
	}
}
void ClientPeer::addPackets(MessagePackets &&m)
{
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::HandleMessagePackets;
	auto packets = new MessagePackets(std::move(m));
	e.user.data1 = packets;
	if (!tryPushEvent(e))
	{
		delete packets;
	}
}
} // namespace TemStream