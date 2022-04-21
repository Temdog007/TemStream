#pragma once

#include <main.hpp>

namespace TemStream
{
class IQuery;
class FileDisplay
{
  private:
	String directory;
	List<String> files;

	void loadFiles();

  public:
	FileDisplay();
	FileDisplay(const String &);
	~FileDisplay();

	const String &getDirectory() const
	{
		return directory;
	}

	const List<String> &getFiles() const
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
class TemStreamGui
{
  private:
	std::array<char, KB(1)> strBuffer;
	WorkQueue workQueue;
	std::optional<Address> connectToServer;
	Map<Message::Source, StreamDisplay> displays;
	Map<Message::Source, shared_ptr<Audio>> audio;
	Message::Streams streams;
	Message::Subscriptions subscriptions;
	PeerInformation peerInfo;
	Mutex peerMutex;
	unique_ptr<ClientConnetion> peer;
	unique_ptr<IQuery> queryData;
#if THREADS_AVAILABLE
	std::thread workThread;
#endif
	std::optional<Message::Source> audioTarget;
	std::optional<FileDisplay> fileDirectory;
	List<String> fontFiles;
	ImGuiIO &io;
	SDL_Window *window;
	SDL_Renderer *renderer;
	const String32 allUTF32;
	float fontSize;
	int fontIndex;
	bool showLogs;
	bool showStreams;
	bool showDisplays;
	bool showAudio;
	bool showFont;
	bool showStats;
	bool streamDirty;

	void LoadFonts();

	void handleMessage(Message::Packet &&);

	void onDisconnect(bool);

	ImVec2 drawMainMenuBar(bool);

	static String32 getAllUTF32();

	friend class TemStreamGuiLogger;

  public:
	TemStreamGui(ImGuiIO &);
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

	WorkQueue &operator*()
	{
		return workQueue;
	}

	bool init();
	bool connect(const Address &);
	void update();
	void doWork();
	void draw();

	bool addAudio(shared_ptr<Audio> &&);
	shared_ptr<Audio> getAudio(const Message::Source &) const;

	bool isConnected();

	void pushFont();

	void setShowLogs(bool v)
	{
		showLogs = v;
	}
	bool isShowingLogs() const
	{
		return showLogs;
	}

	int getSelectedQuery() const;

	void sendPacket(Message::Packet &&, const bool handleLocally = true);
	void sendPackets(MessagePackets &&, const bool handleLocally = true);

	template <typename T> bool operator()(T &)
	{
		return false;
	}

	bool operator()(Message::Streams &s)
	{
		streams = std::move(s);
		*logger << "Got " << streams.size() << " streams from server" << std::endl;
		streamDirty = true;
		return true;
	}

	bool operator()(Message::Subscriptions &s)
	{
		subscriptions = std::move(s);
		*logger << "Got " << subscriptions.size() << " streams from server" << std::endl;
		return true;
	}

	void disconnect();

	static int run();
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
};
} // namespace TemStream