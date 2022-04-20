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
	std::optional<Address> connectToServer;
	Map<MessageSource, StreamDisplay> displays;
	Map<MessageSource, shared_ptr<Audio>> audio;
	PeerInformation peerInfo;
	Mutex peerMutex;
	unique_ptr<ClientPeer> peer;
	unique_ptr<IQuery> queryData;
	std::optional<MessageSource> audioTarget;
	std::optional<FileDisplay> fileDirectory;
	List<String> fontFiles;
	ImGuiIO &io;
	SDL_Window *window;
	SDL_Renderer *renderer;
	const String32 allUTF32;
	float fontSize;
	int fontIndex;
	bool showLogs;
	bool showDisplays;
	bool showAudio;
	bool showFont;
	bool showStats;

	void LoadFonts();

	void handleMessage(MessagePacket &&);

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

	bool init();
	bool connect(const Address &);
	void update();
	void draw();

	bool addAudio(shared_ptr<Audio> &&);
	shared_ptr<Audio> getAudio(const MessageSource &) const;

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

	void sendPacket(MessagePacket &&, const bool handleLocally = true);
	void sendPackets(MessagePackets &&, const bool handleLocally = true);

	static int run();
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