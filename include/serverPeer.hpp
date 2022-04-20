#pragma once

#include <main.hpp>

namespace TemStream
{
class ServerPeer : public Peer
{
  private:
	bool informationAcquired;

	static PeerInformation serverInformation;
	static Mutex peersMutex;
	static List<std::weak_ptr<ServerPeer>> peers;

	static void sendToAllPeers(MessagePacket &&);

	static bool peerExists(const PeerInformation &);

	static void runPeerConnection(shared_ptr<ServerPeer> &&);

	friend class ServerMessageHandler;

  public:
	ServerPeer(const Address &, unique_ptr<Socket>);
	virtual ~ServerPeer();

	bool gotInfo() const
	{
		return informationAcquired;
	}

	bool handlePacket(MessagePacket &&);

	static int run(int, const char **);
};
class ServerMessageHandler
{
  private:
	ServerPeer &server;
	MessagePacket packet;

	bool processCurrentMessage();

  public:
	ServerMessageHandler(ServerPeer &, MessagePacket &&);
	~ServerMessageHandler();

	bool operator()();
	bool operator()(TextMessage &);
	bool operator()(ImageMessage &);
	bool operator()(VideoMessage &);
	bool operator()(AudioMessage &);
	bool operator()(PeerInformation &);
	bool operator()(PeerInformationList &);
	bool operator()(RequestPeers &);
};
} // namespace TemStream