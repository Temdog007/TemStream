#pragma once

#include <main.hpp>

namespace TemStream
{
extern const ImGuiTableFlags TableFlags;
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

	Mutex connectionMutex;
	ConcurrentMap<Message::Source, unique_ptr<AudioSource>> audio;
	ConcurrentMap<Message::Source, shared_ptr<VideoSource>> video;
	ConcurrentMap<Message::Source, shared_ptr<ClientConnection>> connections;

	Map<Message::Source, StreamDisplay> displays;
	Map<Message::Source, unique_ptr<VideoSource::EncoderDecoder>> decodingMap;
	Map<Message::Source, ByteList> pendingVideo;
	Map<Message::Source, int> actionSelections;

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

	void renderConnection(const Message::Source &, shared_ptr<ClientConnection> &);

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

	bool addConnection(const shared_ptr<ClientConnection> &);
	void removeConnection(const Message::Source &);
	bool handleClientConnection(ClientConnection &);

  public:
	shared_ptr<ClientConnection> getConnection(const Message::Source &);
	String getUsername(const Message::Source &);
	size_t getConnectionCount();
	bool hasConnection(const Message::Source &);

  private:
	struct MessageHandler
	{
		TemStreamGui &gui;
		const Message::Source &source;

		bool operator()(Message::Video &);

		bool operator()(Message::ServerInformation &);

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
	bool useAudio(const Message::Source &, const std::function<void(AudioSource &)> &f, const bool create = false);

	bool addVideo(shared_ptr<VideoSource>);

	bool startReplay(const Message::Source &);
	static bool startReplay(ClientConnection &);

	bool getReplays(const Message::Source &, int64_t);
	static bool getReplays(ClientConnection &, int64_t);

	bool hasReplayAccess(const Message::Source &);

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

	bool sendPacket(Message::Packet &&, const bool handleLocally = true);
	bool sendPackets(MessagePackets &&, const bool handleLocally = true);
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
extern void drawAddress(Address &address);

struct PushedID
{
	PushedID(const char *str)
	{
		ImGui::PushID(str);
	}
	~PushedID()
	{
		ImGui::PopID();
	}
};
} // namespace TemStream

template <typename Archive> static inline void serialize(Archive &ar, ImVec4 &v)
{
	ar(cereal::make_nvp("x", v.x), cereal::make_nvp("y", v.y), cereal::make_nvp("z", v.z), cereal::make_nvp("w", v.w));
}
