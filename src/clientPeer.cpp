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
MessageList::MessageList() : messages(), mutex()
{
}
MessageList::~MessageList()
{
}
void MessageList::append(const MessagePacket &packet)
{
	std::lock_guard<std::mutex> lock(mutex);
	messages.push_back(packet);
}
void MessageList::flush(std::vector<MessagePacket> &packet)
{
	std::lock_guard<std::mutex> lock(mutex);
	packet.insert(packet.end(), messages.begin(), messages.end());
	messages.clear();
}
ClientPeer::ClientPeer(MessageList &list, const int fd) : Peer(fd), list(list)
{
}
ClientPeer::~ClientPeer()
{
}
bool ClientPeer::handlePacket(const MessagePacket &packet)
{
	list.append(packet);
	return true;
}
ClientPeerMap::ClientPeerMap() : map(), mutex()
{
}
ClientPeerMap::~ClientPeerMap()
{
}
bool ClientPeerMap::add(const Address &addr, MessageList &list, const int port)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto pair = map.try_emplace(addr, list, port);
	return pair.second;
}
void ClientPeerMap::update()
{
	std::lock_guard<std::mutex> lock(mutex);
	for (auto iter = map.begin(); iter != map.end();)
	{
		if (iter->second.readData(0))
		{
			++iter;
		}
		else
		{
			iter = map.erase(iter);
		}
	}
}
bool ClientPeerMap::forPeer(const Address &addr, const std::function<void(const ClientPeer &)> &f)
{
	std::lock_guard<std::mutex> lock(mutex);
	auto iter = map.find(addr);
	if (iter == map.end())
	{
		return false;
	}
	f(iter->second);
	return true;
}
void ClientPeerMap::forAllPeers(const std::function<void(const std::pair<const Address, ClientPeer> &)> &f)
{
	std::lock_guard<std::mutex> lock(mutex);
	for (const auto &pair : map)
	{
		f(pair);
	}
}
} // namespace TemStream