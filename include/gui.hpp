#pragma once

#include <main.hpp>

namespace TemStream
{
class TemStreamGui
{
  private:
	std::optional<Address> connectToServer;
	std::unique_ptr<ClientPeer> peer;
	std::mutex peerMutex;
	int fontIndex;

  public:
	SDL_Window *window;
	SDL_Renderer *renderer;

	TemStreamGui();
	TemStreamGui(const TemStreamGui &) = delete;
	TemStreamGui(TemStreamGui &&) = delete;

	~TemStreamGui();

	bool init();
	bool connect(const Address &);
	void update();
	void draw(ImGuiIO &);

	void flush(MessagePackets &);

	bool isConnected();

	void pushFont(ImGuiIO &);
};
} // namespace TemStream