#pragma once

#include <main.hpp>

namespace TemStream
{
using MessagePackets = List<MessagePacket>;
class ClientPeer : public Peer
{
  private:
	bool gotInformation;

  public:
	ClientPeer(const Address &, std::unique_ptr<Socket>);
	ClientPeer(const ClientPeer &) = delete;
	ClientPeer(ClientPeer &&) = delete;
	virtual ~ClientPeer();

	bool handlePacket(MessagePacket &&);
	void addPacket(MessagePacket &&);
	void addPackets(MessagePackets &&);

	const Address &getAddress() const
	{
		return address;
	}
};
} // namespace TemStream