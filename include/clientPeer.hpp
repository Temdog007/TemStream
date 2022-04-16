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

	std::unique_ptr<TcpSocket> makeTcpSocket() const;
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
using MessagePackets = std::vector<MessagePacket>;
class ClientPeer : public Peer
{
  private:
	const Address address;
	MessagePackets messages;

  public:
	ClientPeer(const Address &, std::unique_ptr<Socket>);
	ClientPeer(const ClientPeer &) = delete;
	ClientPeer(ClientPeer &&) = delete;
	virtual ~ClientPeer();

	bool handlePacket(const MessagePacket &) override;
	void flush(MessagePackets &);

	const Address &getAddress() const
	{
		return address;
	}
};
} // namespace TemStream