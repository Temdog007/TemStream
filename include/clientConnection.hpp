#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
using MessagePackets = List<Message::Packet>;
class ClientConnetion : public Connection
{
  private:
	TemStreamGui &gui;
	bool acquiredServerInformation;

  public:
	ClientConnetion(TemStreamGui &, const Address &, unique_ptr<Socket>);
	ClientConnetion(const ClientConnetion &) = delete;
	ClientConnetion(ClientConnetion &&) = delete;
	virtual ~ClientConnetion();

	bool flushPackets();
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