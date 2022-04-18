#include <main.hpp>

namespace TemStream
{
ClientPeer::ClientPeer(const Address &address, std::unique_ptr<Socket> s) : Peer(address, std::move(s)), messages()
{
}
ClientPeer::~ClientPeer()
{
}
bool ClientPeer::handlePacket(const MessagePacket &packet)
{
	auto ptr = std::get_if<PeerInformation>(&packet.message);
	if (ptr == nullptr)
	{
		messages.push_back(packet);
	}
	else
	{
		if (ptr->isServer)
		{
			info = *ptr;
		}
		else
		{
			logger->AddError("Connection error. Must connect to a server. Connected to a client");
			return false;
		}
	}
	return true;
}
void ClientPeer::flush(MessagePackets &list)
{
	list.insert(list.end(), messages.begin(), messages.end());
	messages.clear();
}
void ClientPeer::addPacket(const MessagePacket &packet)
{
	messages.push_back(packet);
}
void ClientPeer::addPackets(const MessagePackets &packets)
{
	messages.insert(messages.end(), packets.begin(), packets.end());
}
} // namespace TemStream