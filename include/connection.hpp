#pragma once

#include <main.hpp>

namespace TemStream
{
class Connection
{
  private:
	Bytes bytes;
	std::optional<uint32_t> nextMessageSize;

  protected:
	PeerInformation info;
	const Address address;
	unique_ptr<Socket> mSocket;

  public:
	Connection(const Address &, unique_ptr<Socket>);
	Connection(const Connection &) = delete;
	Connection(Connection &&) = delete;
	~Connection();

	const PeerInformation &getInfo() const
	{
		return info;
	}

	const Address &getAddress() const
	{
		return address;
	}

	Socket *operator->()
	{
		return mSocket.get();
	}

	bool readAndHandle(const int);

	bool isServer() const
	{
		return info.isServer;
	}

	virtual bool handlePacket(Message::Packet &&) = 0;
};

} // namespace TemStream