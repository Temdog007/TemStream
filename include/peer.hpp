#pragma once

#include <main.hpp>

namespace TemStream
{
class Peer : public MessagePacketHandler
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
	virtual ~Peer();

	const PeerInformation &getInfo() const
	{
		return info;
	}

	bool readData(const int timeout = 1);

	bool handlePacket(const MessagePacket &);
};

} // namespace TemStream