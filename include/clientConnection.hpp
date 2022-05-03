#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
using MessagePackets = List<Message::Packet>;
class ClientConnection : public Connection
{
  private:
	TemStreamGui &gui;
	Message::VerifyLogin serverInformation;
	Message::PeerList peers;
	bool opened;

  public:
	ClientConnection(TemStreamGui &, const Address &, unique_ptr<Socket>);
	ClientConnection(const ClientConnection &) = delete;
	ClientConnection(ClientConnection &&) = delete;
	virtual ~ClientConnection();

	bool flushPackets();
	void addPacket(Message::Packet &&);
	void addPackets(MessagePackets &&);

	void close()
	{
		opened = false;
	}

	bool isOpened() const
	{
		return opened;
	}

	const Address &getAddress() const
	{
		return address;
	}

	Message::Source getSource() const;

	const Message::VerifyLogin &getInfo() const
	{
		return serverInformation;
	}

	void setInfo(Message::VerifyLogin &&l)
	{
		serverInformation.swap(l);
	}

	bool operator()(Message::PeerList &&);

	template <typename T> bool operator()(const T &)
	{
		return false;
	}
};
} // namespace TemStream