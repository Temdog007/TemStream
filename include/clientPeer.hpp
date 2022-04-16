#pragma once

#include <main.hpp>

namespace TemStream
{
struct Address
{
	std::string hostname;
	int port;

	Address();
	~Address();

	bool operator==(const Address &) const;

	std::optional<int> makeSocket() const;
};
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::Address>
{
	std::size_t operator()(const TemStream::Address &addr) const
	{
		return hash<string>()(addr.hostname) ^ hash<int>()(addr.port);
	}
};
} // namespace std
namespace TemStream
{
class MessageList
{
  private:
	std::vector<MessagePacket> messages;
	std::mutex mutex;

  public:
	MessageList();
	~MessageList();

	void append(const MessagePacket &);
	void flush(std::vector<MessagePacket> &);
};
class ClientPeer : public Peer
{
  private:
	MessageList &list;

  public:
	ClientPeer(MessageList &, int);
	ClientPeer(ClientPeer &&);
	virtual ~ClientPeer();

	bool handlePacket(const MessagePacket &) override;
};
class ClientPeerMap
{
  private:
	std::unordered_map<Address, ClientPeer> map;
	std::mutex mutex;

  public:
	ClientPeerMap();
	~ClientPeerMap();

	bool add(const Address &, MessageList &, int);
	void update();

	bool forPeer(const Address &, const std::function<void(const ClientPeer &)> &);
	void forAllPeers(const std::function<void(const std::pair<const Address, ClientPeer> &)> &);
};
} // namespace TemStream