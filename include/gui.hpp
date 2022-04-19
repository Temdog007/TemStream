#pragma once

#include <main.hpp>

namespace TemStream
{
class IQuery;
class TemStreamGui
{
  private:
	std::optional<Address> connectToServer;
	Map<MessageSource, StreamDisplay> displays;
	Map<MessageSource, std::shared_ptr<Audio>> audio;
	PeerInformation peerInfo;
	Mutex peerMutex;
	MessagePackets outgoingPackets;
	std::unique_ptr<ClientPeer> peer;
	std::unique_ptr<IQuery> queryData;
	List<String> fontFiles;
	ImGuiIO &io;
	float fontSize;
	int fontIndex;
	bool showLogs;
	bool showAudio;
	bool showFont;
	ImGuiWindowFlags streamDisplayFlags;

	void LoadFonts();

	ImVec2 drawMainMenuBar(bool);

  public:
	SDL_Window *window;
	SDL_Renderer *renderer;

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

	ImGuiWindowFlags getStreamDisplayFlags() const
	{
		return streamDisplayFlags;
	}

	bool init();
	bool connect(const Address &);
	void update();
	void draw();

	bool addAudio(std::shared_ptr<Audio>);
	std::shared_ptr<Audio> getAudio(const MessageSource &) const;

	void flush(MessagePackets &);

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

	void sendPacket(const MessagePacket &, const bool handleLocally = true);
	void sendPackets(const MessagePackets &, const bool handleLocally = true);

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