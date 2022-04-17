#pragma once

#include <main.hpp>

namespace TemStream
{
extern int runServer(int, const char **);

class ServerPeer : public Peer, public MessagePacketHandler
{
  private:
	bool informationAcquired;

  public:
	ServerPeer(const Address &, std::unique_ptr<Socket>);
	virtual ~ServerPeer();

	bool gotInfo() const
	{
		return informationAcquired;
	}

	bool handlePacket(const MessagePacket &) override;
	virtual bool operator()(const TextMessage &);
	virtual bool operator()(const ImageMessage &);
	virtual bool operator()(const VideoMessage &);
	virtual bool operator()(const AudioMessage &);
	virtual bool operator()(const PeerInformation &);
	virtual bool operator()(const PeerInformationList &);
	virtual bool operator()(const RequestPeers &);
};
} // namespace TemStream