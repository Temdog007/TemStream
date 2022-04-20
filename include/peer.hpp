#pragma once

#include <main.hpp>

namespace TemStream
{
class Peer
{
  private:
	Bytes bytes;
	std::optional<uint32_t> nextMessageSize;

  protected:
	PeerInformation info;
	const Address address;
	unique_ptr<Socket> mSocket;

  public:
	Peer(const Address &, unique_ptr<Socket>);
	Peer(const Peer &) = delete;
	Peer(Peer &&) = delete;
	virtual ~Peer();

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

	virtual bool handlePacket(MessagePacket &&) = 0;
};

} // namespace TemStream