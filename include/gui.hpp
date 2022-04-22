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
	friend int runApp(Configuration &);

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
	const String32 allUTF32;
	ImGuiIO &io;
	Configuration &configuration;
	SDL_Window *window;
	SDL_Renderer *renderer;
	bool streamDirty;

	void LoadFonts();

	void handleMessage(Message::Packet &&);

	void onDisconnect(bool);

	ImVec2 drawMainMenuBar(bool);

	static String32 getAllUTF32();

	friend class TemStreamGuiLogger;

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
		configuration.showLogs = v;
	}
	bool isShowingLogs() const
	{
		return configuration.showLogs;
	}

	int getSelectedQuery() const;

	void sendPacket(Message::Packet &&, const bool handleLocally = true);
	void sendPackets(MessagePackets &&, const bool handleLocally = true);

	bool operator()(const Message::Streams &s);
	bool operator()(const Message::Subscriptions &s);
	template <typename T> bool operator()(const T &)
	{
#if LOG_MESSAGE_TYPE
		int status;
		char *realname = abi::__cxa_demangle(typeid(T).name(), 0, 0, &status);
		std::cout << "Ignoring -> " << realname << ':' << status << std::endl;
		free(realname);
#endif
		return false;
	}

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
};
} // namespace TemStream