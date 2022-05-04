#pragma once

#include <main.hpp>

namespace TemStream
{
class ServerConnection : public Connection
{
	friend int runApp(Configuration &configuration);

  private:
	static std::atomic_int32_t runningThreads;
	static Configuration configuration;
	static Mutex peersMutex;
	static LinkedList<std::weak_ptr<ServerConnection>> peers;

	static void sendToPeers(Message::Packet &&, const ServerConnection *author = nullptr);

	static bool peerExists(const String &);

	static size_t totalPeers();

	static StringList getPeers();

	static void sendLinks(const String &);

	static void sendLinks();

	static void checkAccess();

	static std::optional<PeerInformation> login(const Message::Credentials &);

	static void runPeerConnection(shared_ptr<ServerConnection>);

	typedef bool (*VerifyToken)(const char *, char *, uint8_t *);
	typedef bool (*VerifyUsernameAndPassword)(const char *, const char *, char *, uint8_t *);

	class CredentialHandler
	{
	  private:
		VerifyToken verifyToken;
		VerifyUsernameAndPassword verifyUsernameAndPassword;

	  public:
		CredentialHandler(VerifyToken, VerifyUsernameAndPassword);
		~CredentialHandler();

		std::optional<PeerInformation> operator()(const String &);
		std::optional<PeerInformation> operator()(const Message::UsernameAndPassword &);
	};

	class MessageHandler
	{
	  private:
		ServerConnection &connection;
		Message::Packet packet;

		bool processCurrentMessage();

		bool savePayloadIfNedded(bool append = false) const;

		bool sendStoredPayload();

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

		void operator()(const Message::LargeFile &);
		void operator()(std::monostate);
		void operator()(const ByteList &);
		void operator()(uint64_t);
	};

	shared_ptr<ServerConnection> getPointer() const;

	void handleInput();
	void handleOutput();

	PeerInformation information;
	const TimePoint startingTime;
	TimePoint lastMessage;
	bool stayConnected;

  public:
	ServerConnection(Address &&, unique_ptr<Socket>);
	ServerConnection(const ServerConnection &) = delete;
	ServerConnection(ServerConnection &&) = delete;
	~ServerConnection();

	static Message::Source getSource();

	bool isAuthenticated() const;

	template <const size_t N> static void getFilename(std::array<char, N> &arr)
	{
		snprintf(arr.data(), arr.size(), "%s_%u_%" PRId64 ".tsd", configuration.name.c_str(), configuration.serverType,
				 configuration.startTime);
	}
};
} // namespace TemStream