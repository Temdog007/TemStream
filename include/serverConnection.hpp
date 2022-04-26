#pragma once

#include <main.hpp>

namespace TemStream
{
class ServerConnection : public Connection
{
	friend int runApp(Configuration &configuration);

  private:
	Message::Subscriptions subscriptions;
	bool informationAcquired;

	static std::atomic_int32_t runningThreads;
	static Message::Streams streams;
	static Configuration configuration;
	static Mutex peersMutex;
	static List<std::weak_ptr<ServerConnection>> peers;
	static Message::PeerInformationSet peersFromOtherServers;

	enum Target
	{
		Client = 1 << 0,
		Server = 1 << 1,
		Both = Client | Server
	};

	static bool sendToPeers(Message::Packet &&, const Target t = Target::Both, const bool checkSubscription = true);

	static bool peerExists(const PeerInformation &);

	static size_t totalPeers();

	static size_t totalStreams();

	static std::optional<Stream> getStream(const Message::Source &);

	static void runPeerConnection(shared_ptr<ServerConnection>);

	static Message::PeerInformationSet getPeers();

	static std::optional<PeerInformation> getPeerFromCredentials(Message::Credentials &&);

	shared_ptr<ServerConnection> getPointer() const;

	typedef bool (*VerifyToken)(const char *, char *, bool *);
	typedef bool (*VerifyUsernameAndPassword)(const char *, const char *, char *, bool *);

	class CredentialHandler
	{
	  private:
		VerifyToken verifyToken;
		VerifyUsernameAndPassword verifyUsernameAndPassword;

	  public:
		CredentialHandler(VerifyToken, VerifyUsernameAndPassword);
		~CredentialHandler();

		std::optional<PeerInformation> operator()(String &&);
		std::optional<PeerInformation> operator()(Message::UsernameAndPassword &&);
	};

	class MessageHandler
	{
	  private:
		ServerConnection &connection;
		Message::Packet packet;

		bool processCurrentMessage(const Target t = Target::Both, const bool checkSubscription = true);

		bool sendSubscriptionsToClient() const;

		bool sendStreamsToClients() const;

		bool savePayloadIfNedded(bool append = false) const;

		bool sendPayloadForStream(const Message::Source &);

		static void sendImageBytes(shared_ptr<ServerConnection>, Message::Source &&, String &&filename);

	  public:
		MessageHandler(ServerConnection &, Message::Packet &&);
		~MessageHandler();

		bool operator()();
		MESSAGE_HANDLER_FUNCTIONS(bool);
	};

	class ImageSaver
	{
	  private:
		ServerConnection &connection;
		const Message::Source &source;

	  public:
		ImageSaver(ServerConnection &, const Message::Source &);
		~ImageSaver();

		void operator()(std::monostate);
		void operator()(const ByteList &);
		void operator()(uint64_t);
	};

  public:
	ServerConnection(const Address &, unique_ptr<Socket>);
	virtual ~ServerConnection();

	bool gotInfo() const
	{
		return informationAcquired;
	}

	bool handlePacket(Message::Packet &&) override;
};
} // namespace TemStream