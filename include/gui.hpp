#pragma once

#include <main.hpp>

namespace TemStream
{
class IQuery;
class FileDisplay
{
  private:
	String directory;
	StringList files;

	void loadFiles();

  public:
	FileDisplay();
	FileDisplay(const String &);
	~FileDisplay();

	const String &getDirectory() const
	{
		return directory;
	}

	const StringList &getFiles() const
	{
		return files;
	}
};
class SDL_MemoryFunctions
{
  private:
	SDL_malloc_func mallocFunc;
	SDL_calloc_func callocFunc;
	SDL_realloc_func reallocFunc;
	SDL_free_func freeFunc;

  public:
	SDL_MemoryFunctions();

	void SetToSDL() const;
	void GetFromSDL();
};
using VideoPacket = std::pair<Message::Source, Message::Video>;
class TemStreamGui
{
	friend int runApp(Configuration &);

  private:
	std::array<char, KB(1)> strBuffer;
	std::optional<Address> connectToServer;
	Map<Message::Source, StreamDisplay> displays;
	Map<Message::Source, unique_ptr<Audio>> audio;
	Map<Message::Source, shared_ptr<Video>> video;
	Map<Message::Source, unique_ptr<Video::EncoderDecoder>> decodingMap;
	Map<Message::Source, ByteList> pendingVideo;
	ConcurrentQueue<VideoPacket> videoPackets;
	Message::Streams streams;
	Message::Subscriptions subscriptions;
	Message::PeerInformationSet otherPeers;
	PeerInformation peerInfo;
	shared_ptr<ClientConnetion> clientConnection;
	std::weak_ptr<ClientConnetion> updatePeerCon;
	std::weak_ptr<ClientConnetion> flushPacketsCon;
	std::weak_ptr<ClientConnetion> mainThreadCon;
	unique_ptr<IQuery> queryData;
	std::optional<Message::Source> audioTarget;
	std::optional<FileDisplay> fileDirectory;
	const String32 allUTF32;
	TimePoint lastVideoCheck;
	ImGuiIO &io;
	Configuration &configuration;
	SDL_Window *window;
	SDL_Renderer *renderer;
	bool dirty;

	void LoadFonts();

	void handleMessage(Message::Packet &&);

	void onDisconnect(bool);

	ImVec2 drawMainMenuBar(bool);

	static String32 getAllUTF32();

	friend class TemStreamGuiLogger;

	struct RenderCredentials
	{
		void operator()(String &) const;
		void operator()(Message::UsernameAndPassword &) const;
	};

	bool init();
	bool connect(const Address &);
	void updatePeer();
	void decodeVideoPackets();
	void draw();
	void refresh();

	struct MessageHandler
	{
		TemStreamGui &gui;
		const Message::Source &source;

		bool operator()(Message::Streams &s);
		bool operator()(Message::VerifyLogin &);
		bool operator()(Message::PeerInformationSet &);
		bool operator()(Message::Subscriptions &);
		bool operator()(Message::Video &);

		template <typename T> bool operator()(T &)
		{
#if LOG_MESSAGE_TYPE
			int status;
			char *realname = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
			std::cout << "Ignoring -> " << realname << ':' << status << std::endl;
			free(realname);
#endif
			return false;
		}
	};

  public:
	TemStreamGui(ImGuiIO &, Configuration &);
	TemStreamGui(const TemStreamGui &) = delete;
	TemStreamGui(TemStreamGui &&) = delete;

	~TemStreamGui();

	const PeerInformation &getInfo() const
	{
		return peerInfo;
	}

	ImGuiIO &getIO()
	{
		return io;
	}

	SDL_Renderer *getRenderer()
	{
		return renderer;
	}
	SDL_Window *getWindow()
	{
		return window;
	}

	const Configuration &getConfiguration() const
	{
		return configuration;
	}

	bool addAudio(unique_ptr<Audio> &&);
	bool useAudio(const Message::Source &, const std::function<void(Audio &)> &f);

	bool addVideo(shared_ptr<Video>);

	bool isConnected();

	void pushFont();

	void setShowLogs(bool v)
	{
		configuration.showLogs = v;
	}
	bool isShowingLogs() const
	{
		return configuration.showLogs;
	}

	int getSelectedQuery() const;

	void sendPacket(Message::Packet &&, const bool handleLocally = true);
	void sendPackets(MessagePackets &&, const bool handleLocally = true);

	void disconnect();

	static bool sendCreateMessage(const Message::Source &, uint32_t);

	template <typename T> static bool sendCreateMessage(const Message::Source &source)
	{
		return sendCreateMessage(source, variant_index<Message::Payload, T>());
	}

	static bool sendCreateMessage(const Message::Packet &);
};
class TemStreamGuiLogger : public InMemoryLogger
{
  private:
	TemStreamGui &gui;

	void checkError(Level);

  protected:
	virtual void Add(Level, const String &, bool) override;

  public:
	TemStreamGuiLogger(TemStreamGui &);
	~TemStreamGuiLogger();

	void saveLogs();
};
} // namespace TemStream

template <typename Archive> static inline void serialize(Archive &ar, ImVec4 &v)
{
	ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}
