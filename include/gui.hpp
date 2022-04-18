#pragma once

#include <main.hpp>

namespace TemStream
{
class IQuery;
class TemStreamGui
{
  private:
	std::optional<Address> connectToServer;
	std::optional<String> pendingFile;
	Map<MessageSource, StreamDisplay> displays;
	PeerInformation peerInfo;
	Mutex peerMutex;
	MessagePackets outgoingPackets;
	std::unique_ptr<ClientPeer> peer;
	std::unique_ptr<IQuery> queryData;
	ImGuiIO &io;
	int fontIndex;

	friend int runGui();

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

	bool init();
	bool connect(const Address &);
	void update();
	void draw();

	void flush(MessagePackets &);

	bool isConnected();

	void pushFont();

	bool handleFile(const char *);

	bool handleText(const char *);

	int getSelectedQuery() const;

	void sendPacket(const MessagePacket &, const bool handleLocally = true);
	void sendPackets(const MessagePackets &, const bool handleLocally = true);
};
} // namespace TemStream