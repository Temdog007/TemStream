#pragma once

#include <main.hpp>

namespace TemStream
{
using MessagePackets = List<MessagePacket>;
class ClientPeer : public Peer
{
  private:
	bool acquiredServerInformation;

  public:
	ClientPeer(const Address &, unique_ptr<Socket>);
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

	bool gotServerInformation() const
	{
		return acquiredServerInformation;
	}
};
} // namespace TemStream