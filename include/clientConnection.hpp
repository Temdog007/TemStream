#pragma once

#include <main.hpp>

namespace TemStream
{
using MessagePackets = List<Message::Packet>;
class ClientConnetion : public Connection
{
  private:
	bool acquiredServerInformation;

  public:
	ClientConnetion(const Address &, unique_ptr<Socket>);
	ClientConnetion(const ClientConnetion &) = delete;
	ClientConnetion(ClientConnetion &&) = delete;
	virtual ~ClientConnetion();

	bool handlePacket(Message::Packet &&) override;
	void addPacket(Message::Packet &&);
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