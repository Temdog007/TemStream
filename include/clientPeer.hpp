#pragma once

#include <main.hpp>

namespace TemStream
{
using MessagePackets = std::vector<MessagePacket>;
class ClientPeer : public Peer
{
  private:
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