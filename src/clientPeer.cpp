#include <main.hpp>

namespace TemStream
{
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
} // namespace TemStream