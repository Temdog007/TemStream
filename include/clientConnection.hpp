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
	Message::VerifyLogin verifyLogin;
	Message::ServerInformation serverInformation;
	TimePoint lastSentMessage;
	bool opened;

  public:
	ClientConnection(TemStreamGui &, const Address &, unique_ptr<Socket>);
	ClientConnection(const ClientConnection &) = delete;
	ClientConnection(ClientConnection &&) = delete;
	virtual ~ClientConnection();

	bool sendPacket(const Message::Packet &, const bool sendImmediately = false);
	bool flushPackets();
	void addPacket(Message::Packet &&);
	void addPackets(MessagePackets &&);

	void close();

	bool isOpened() const
	{
		return opened;
	}

	const Address &getAddress() const
	{
		return address;
	}

	Message::Source getSource() const;

	std::optional<std::chrono::duration<double>> nextSendInterval() const;

	const Message::VerifyLogin &getInfo() const
	{
		return verifyLogin;
	}

	void setVerifyLogin(Message::VerifyLogin &&verifyLogin)
	{
		this->verifyLogin = std::move(verifyLogin);
	}

	const Message::ServerInformation &getServerInformation() const
	{
		return serverInformation;
	}

	bool setServerInformation(Message::ServerInformation &&serverInformation)
	{
		this->serverInformation = std::move(serverInformation);
		return true;
	}
};
} // namespace TemStream