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
	PeerInformation peerInfo;
	Mutex peerMutex;
	MessagePackets outgoingPackets;
	std::unique_ptr<ClientPeer> peer;
	std::unique_ptr<IQuery> queryData;
	ImGuiIO &io;
	int fontIndex;
	bool showLogs;
	ImGuiWindowFlags streamDisplayFlags;

	friend int runGui();

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

	void flush(MessagePackets &);

	bool isConnected();

	void pushFont();

	void setShowLogs(bool v)
	{
		showLogs = v;
	}

	int getSelectedQuery() const;

	void sendPacket(const MessagePacket &, const bool handleLocally = true);
	void sendPackets(const MessagePackets &, const bool handleLocally = true);
};
class TemStreamGuiLogger : public InMemoryLogger
{
  private:
	TemStreamGui &gui;

  public:
	TemStreamGuiLogger(TemStreamGui &);
	~TemStreamGuiLogger();

	void Add(Level, const char *, va_list) override;
};
} // namespace TemStream