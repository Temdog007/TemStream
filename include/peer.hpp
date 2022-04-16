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
	std::unique_ptr<Socket> mSocket;

  public:
	Peer(std::unique_ptr<Socket> &&);
	Peer(const Peer &) = delete;
	Peer(Peer &&) = delete;
	virtual ~Peer();

	const PeerInformation &getInfo() const
	{
		return info;
	}

	Socket *operator->()
	{
		return mSocket.get();
	}

	bool readAndHandle(const int);

	virtual bool handlePacket(const MessagePacket &) = 0;
};

} // namespace TemStream