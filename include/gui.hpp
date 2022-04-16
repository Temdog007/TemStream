#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui
{
  private:
	std::optional<Address> connectToServer;
	std::unique_ptr<ClientPeer> peer;
	std::optional<std::string> pendingFile;
	std::mutex peerMutex;
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
};
} // namespace TemStream