#pragma once

#include <main.hpp>

namespace TemStream
{
class Peer
{
  private:
	std::array<char, KB(8)> buffer;
	Bytes data;
	std::optional<uint32_t> nextMessageSize;

	void close();

  protected:
	PeerInformation info;
	int fd;

  public:
	Peer(int);
	Peer(const Peer &) = delete;
	virtual ~Peer();

	const PeerInformation &getInfo() const
	{
		return info;
	}

	bool readData(const int timeout = 1);

	virtual bool handlePacket(const MessagePacket &) = 0;
};

} // namespace TemStream