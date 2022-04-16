#include <main.hpp>

namespace TemStream
{
Address::Address() : hostname("localhost"), port(DefaultPort)
{
}
Address::~Address()
{
}
bool Address::operator==(const Address &a) const
{
	return port == a.port && hostname == a.hostname;
}
std::optional<int> Address::makeSocket() const
{
	int fd = -1;
	char portStr[64];
	snprintf(portStr, sizeof(portStr), "%d", port);
	if (openSocket(fd, hostname.c_str(), portStr, false))
	{
		return fd;
	}
	close(fd);
	return std::nullopt;
}
ClientPeer::ClientPeer(const Address &address, const int fd) : Peer(fd), address(address), messages()
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
void ClientPeer::flush(std::vector<MessagePacket> &list)
{
	list.insert(list.end(), messages.begin(), messages.end());
	messages.clear();
}
} // namespace TemStream