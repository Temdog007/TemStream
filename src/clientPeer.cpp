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
		messages.emplace_back(packet);
	}
	else
	{
		if (ptr->isServer)
		{
			info = *ptr;
		}
		else
		{
			SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Connection error",
									 "Must connect to a server. Connected to a client", NULL);
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
} // namespace TemStream