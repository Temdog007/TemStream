#pragma once

#include <main.hpp>

namespace TemStream
{
extern const ImGuiTableFlags TableFlags;
class IQuery;

/**
 * For displaying files in a directory.
 */
class FileDisplay
{
  private:
	String directory;
	StringList files;

	void loadFiles();

  public:
	FileDisplay();
	FileDisplay(const String &);
	template <class S> FileDisplay(const S &s) : directory(s), files()
	{
		loadFiles();
	}
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
/**
 * Store all memory functions that can be set for usage by SDL2
 */
class SDL_MemoryFunctions
{
  private:
	SDL_malloc_func mallocFunc;
	SDL_calloc_func callocFunc;
	SDL_realloc_func reallocFunc;
	SDL_free_func freeFunc;

  public:
	SDL_MemoryFunctions();

	/**
	 * Set all of the memory functions for SDL to this object's memory functions
	 */
	void SetToSDL() const;

	/**
	 * Get and store all of the memory functions that SDL is currently using
	 */
	void GetFromSDL();
};
using VideoPacket = std::pair<Message::Source, Message::Video>;
class TemStreamGui
{
  private:
	friend int runApp(Configuration &);
	friend void runLoop(TemStreamGui &);

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

	// If set to true, need to check the state of all video, audio, and stream displays and remove those that are
	// invalid
	bool dirty;

	void LoadFonts();

	/**
	 * Handle a message received from the server
	 *
	 * @param packet
	 */
	void handleMessage(Message::Packet &&packet);

	ImVec2 drawMainMenuBar();

	/**
	 * Render a connection to a server and display options (upload, disconnect, etc.)
	 *
	 * @param source
	 * @param connection
	 */
	void renderConnection(const Message::Source &source, shared_ptr<ClientConnection> &connection);

	static String32 getAllUTF32();

	friend class TemStreamGuiLogger;

	struct RenderCredentials
	{
		void operator()(String &) const;
		void operator()(Message::UsernameAndPassword &) const;
	};

	/**
	 * Includes operations like initializing SDL and starting async operations
	 *
	 * @return True if successful
	 */
	bool init();

	/**
	 * Decode the video packets currently in the video packet list and send them to then event loop to be displayed
	 */
	void decodeVideoPackets();

	/**
	 * All draw calls are handled by this function
	 */
	void draw();

	bool addConnection(const shared_ptr<ClientConnection> &);
	void removeConnection(const Message::Source &);

	/**
	 * Read incoming packets from this connection and send enqueued packets to the peer
	 *
	 * @param con
	 *
	 * @return False, if the connection has been closed
	 */
	bool handleClientConnection(ClientConnection &con);

	/**
	 * Push the font based on the index to ImGui
	 */
	void pushFont();

	void clearAll();

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

	/**
	 * Find audio with the source and call the function with it
	 *
	 * @param source
	 * @param func
	 * @param create If audio with source doesn't exist, create new audio source. Newly created audio source will always
	 * be of type playback
	 *
	 * @return False if audio source doesn't exist and create == false
	 */
	bool useAudio(const Message::Source &, const std::function<void(AudioSource &)> &func, const bool create = false);

	bool addVideo(shared_ptr<VideoSource>);

	/**
	 * Start replaying packets for this source. This function will send the GetTimeRange message to the server in which
	 * the server should reply with the TimeRange message. When the TimeRange message is received, the StreamDisplay
	 * will be put into replay mode.
	 *
	 * @param source
	 *
	 * @return True if the message was sent successfully
	 */
	bool startReplay(const Message::Source &);

	/**
	 * See TemStreamGui::startReplay(const Message::Source&)
	 *
	 * @param connection
	 *
	 * @return True if successful
	 */
	static bool startReplay(ClientConnection &);

	/**
	 * Get one second of replays within a time frame from the server
	 *
	 * @param source
	 * @param timestamp
	 *
	 * @return True if the request was sent successfully
	 */
	bool getReplays(const Message::Source &, int64_t);

	/**
	 * See TemStreamGui::getReplays(const Message::Source &, int64_t)
	 *
	 * @param connection
	 * @param timestamp
	 *
	 * @return True is successful
	 */
	static bool getReplays(ClientConnection &, int64_t);

	bool hasReplayAccess(const Message::Source &);

	void connect(const Address &);

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

	void setQuery(ServerType, const Message::Source &);

	bool sendPacket(Message::Packet &&, const bool handleLocally = true);
	bool sendPackets(MessagePackets &&, const bool handleLocally = true);

	void cleanupIfDirty();
};
class TemStreamGuiLogger : public InMemoryLogger
{
  private:
	TemStreamGui &gui;

	/**
	 * Check if the log is an error. If so and the log window is not open, display message box about the error and open
	 * log window.
	 */
	void checkError(Level);

  protected:
	virtual void Add(Level, const String &, bool) override;

  public:
	TemStreamGuiLogger(TemStreamGui &);
	~TemStreamGuiLogger();

	/**
	 * Save logs to a file
	 */
	void saveLogs();
};

template <typename S> void drawAddress(BaseAddress<S> &address)
{
	ImGui::InputText("Hostname", &address.hostname);
	if (ImGui::InputInt("Port", &address.port, 1, 100))
	{
		address.port = std::clamp(address.port, 1, static_cast<int>(UINT16_MAX));
	}
}

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
