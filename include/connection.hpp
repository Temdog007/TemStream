#pragma once

#include <main.hpp>

namespace TemStream
{
class Connection
{
  private:
	ByteList bytes;
	ConcurrentQueue<Message::Packet> packets;
	std::optional<uint32_t> nextMessageSize;

  protected:
	const Address address;
	unique_ptr<Socket> mSocket;
	size_t maxMessageSize;

  public:
	Connection(const Address &, unique_ptr<Socket>);
	Connection(const Connection &) = delete;
	Connection(Connection &&) = delete;
	virtual ~Connection();

	const Address &getAddress() const
	{
		return address;
	}

	Socket *operator->()
	{
		return mSocket.get();
	}

	ConcurrentQueue<Message::Packet> &getPackets()
	{
		return packets;
	}

	bool readAndHandle(const int);
};

} // namespace TemStream