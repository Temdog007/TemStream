#pragma once

#include <main.hpp>

namespace TemStream
{
class IQuery;
class TemStreamGui
{
  private:
	std::optional<Address> connectToServer;
	std::optional<std::string> pendingFile;
	PeerInformation peerInfo;
	std::mutex peerMutex;
	MessagePackets outgoingPackets;
	MessagePackets incomingPackets;
	std::unique_ptr<ClientPeer> peer;
	std::unique_ptr<IQuery> queryData;
	ImGuiIO &io;
	int fontIndex;

  public:
	SDL_Window *window;
	SDL_Renderer *renderer;

	TemStreamGui(ImGuiIO &);
	TemStreamGui(const TemStreamGui &) = delete;
	TemStreamGui(TemStreamGui &&) = delete;

	~TemStreamGui();

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