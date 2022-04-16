#pragma once

#include <main.hpp>

namespace TemStream
{
class MessageList
{
  private:
	std::vector<MessagePacket> messages;
	std::mutex mutex;

  public:
	MessageList();
	~MessageList();

	void append(const MessagePacket &);
	void flush(std::vector<MessagePacket> &);
};
class ClientPeer : public Peer
{
  private:
	MessageList &list;

  public:
	ClientPeer(MessageList &, int);
	virtual ~ClientPeer();

	bool handlePacket(const MessagePacket &) override;
};
} // namespace TemStream