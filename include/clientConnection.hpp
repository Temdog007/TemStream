#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui;
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

	/**
	 * Packets are enqueued in a list. The will be sent when ::flushPackets() is called
	 *
	 * @param packet
	 * @param sendImmediately Will call flush if true
	 *
	 * @return True if the flush call is successful (if it was called). Otherwise, always true
	 */
	bool sendPacket(const Message::Packet &packet, const bool sendImmediately = false);

	/**
	 * Send all packets in the outgoing list to the server
	 *
	 * @return True if sent successfully
	 */
	bool flushPackets();

	/**
	 * Add packet to the list of incoming packets. All packets from the server will call this method
	 *
	 * @param packet
	 */
	void addPacket(Message::Packet &&packet);

	/**
	 * Same as ::addPacket but for all packets in the list
	 *
	 * @param packets
	 */
	void addPackets(MessagePackets &&packets);

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

	/**
	 * Some servers (most likely chat servers), will have a send interval. If the client sends a packet faster than the
	 * send interval, they will be disconnected. That number is received from the server on login.
	 *
	 * @return the send interval
	 */
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