#pragma once

#include <main.hpp>

namespace TemStream
{
struct RecordedPacket
{
	Message::Packet packet;
	int64_t timestamp;

	RecordedPacket(const Message::Packet &);
	~RecordedPacket();

	bool save(const String &filename) const;

	static std::optional<int64_t> getTimestamp(const String &s, std::string::size_type &pos);
	static std::optional<String> getEncodedPacket(const String &s, const int64_t target);
};
class ServerConnection : public Connection
{
	friend int runApp(Configuration &configuration);

  private:
	static std::atomic_int32_t runningThreads;
	static Mutex peersMutex;
	static unique_ptr<StringList> badWords;
	static unique_ptr<LinkedList<std::weak_ptr<ServerConnection>>> peers;

	static void sendToPeers(Message::Packet &&, const ServerConnection *author = nullptr);

	static bool peerExists(const String &);

	static size_t totalPeers();

	static List<PeerInformation> getPeers();

	static String sendLinks(Configuration &);

	static void checkAccess(Configuration &);

	std::optional<PeerInformation> login(const Message::Credentials &);

	static void runPeerConnection(shared_ptr<ServerConnection>);

	static String getReplayFilename(Configuration &);

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
	Configuration &configuration;
	ConcurrentQueue<RecordedPacket> &packetsToRecord;
	bool stayConnected;

  public:
	ServerConnection(Configuration &, ConcurrentQueue<RecordedPacket> &, Address &&, unique_ptr<Socket>);
	ServerConnection(const ServerConnection &) = delete;
	ServerConnection(ServerConnection &&) = delete;
	~ServerConnection();

	bool isAuthenticated() const;

	template <const size_t N> void getFilename(std::array<char, N> &arr)
	{
		snprintf(arr.data(), arr.size(), "%s_%u_%" PRId64 ".tsd", configuration.name.c_str(),
				 (uint32_t)configuration.serverType, configuration.startTime);
	}
};
} // namespace TemStream