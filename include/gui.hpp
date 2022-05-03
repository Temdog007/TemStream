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
	Map<Message::Source, StreamDisplay> displays;
	Map<Message::Source, unique_ptr<AudioSource>> audio;
	Map<Message::Source, shared_ptr<VideoSource>> video;
	Map<Message::Source, unique_ptr<VideoSource::EncoderDecoder>> decodingMap;
	Map<Message::Source, ByteList> pendingVideo;
	Map<Message::Source, std::weak_ptr<ClientConnection>> connections;
	Mutex connectionMutex;
	ConcurrentQueue<VideoPacket> videoPackets;
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

	ImVec2 drawMainMenuBar();

	static String32 getAllUTF32();

	friend class TemStreamGuiLogger;

	struct RenderCredentials
	{
		void operator()(String &) const;
		void operator()(Message::UsernameAndPassword &) const;
	};

	bool init();

	void decodeVideoPackets();
	void draw();

	void addConnection(const shared_ptr<ClientConnection> &);
	shared_ptr<ClientConnection> getConnection(const Message::Source &);
	size_t getConnectionCount();
	bool hasConnection(const Message::Source &);
	void forEachConnection(const std::function<void(ClientConnection &)> &);
	void removeConnection(const Message::Source &);

	static bool handleClientConnection(ClientConnection &);

	struct MessageHandler
	{
		TemStreamGui &gui;
		const Message::Source &source;

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

	bool addAudio(unique_ptr<AudioSource> &&);
	bool useAudio(const Message::Source &, const std::function<void(AudioSource &)> &f);

	bool addVideo(shared_ptr<VideoSource>);

	void connect(const Address &);

	void pushFont();

	void setShowLogs(bool v)
	{
		configuration.showLogs = v;
	}
	bool isShowingLogs() const
	{
		return configuration.showLogs;
	}

	static ServerType getSelectedQuery(const IQuery *);
	unique_ptr<IQuery> getQuery(ServerType, const Message::Source &);

	void sendPacket(Message::Packet &&, const bool handleLocally = true);
	void sendPackets(MessagePackets &&, const bool handleLocally = true);
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
