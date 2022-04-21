#pragma once

#include <main.hpp>

namespace TemStream
{
class ServerConnection : public Connection
{
  private:
	Message::Subscriptions subscriptions;
	bool informationAcquired;

	static Message::Streams streams;
	static PeerInformation serverInformation;
	static Mutex peersMutex;
	static List<std::weak_ptr<ServerConnection>> peers;

	enum Target
	{
		Client = 1 << 0,
		Server = 1 << 1,
		Both = Client | Server
	};

	static bool sendToPeers(Message::Packet &&, const Target t = Target::Both, const bool checkSubscription = true);

	static bool peerExists(const PeerInformation &);

	static std::optional<Stream> getStream(const Message::Source &);

	static void runPeerConnection(shared_ptr<ServerConnection> &&);

	class MessageHandler
	{
	  private:
		ServerConnection &connection;
		Message::Packet packet;

		bool processCurrentMessage(const Target t = Target::Both, const bool checkSubscription = true);

		bool sendSubscriptionsToClient() const;

		bool sendStreamsToClients() const;

		bool savePayloadIfNedded() const;

		static std::optional<Message::Payload> loadPayloadForStream(const Message::Source &);

	  public:
		MessageHandler(ServerConnection &, Message::Packet &&);
		~MessageHandler();

		bool operator()();
		MESSAGE_HANDLER_FUNCTIONS(bool);
	};

  public:
	ServerConnection(const Address &, unique_ptr<Socket>);
	virtual ~ServerConnection();

	bool gotInfo() const
	{
		return informationAcquired;
	}

	bool handlePacket(Message::Packet &&) override;

	static int run(int, const char **);
};
} // namespace TemStream