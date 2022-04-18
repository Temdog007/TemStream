#include <main.hpp>

namespace TemStream
{
// IQuery
IQuery::IQuery(TemStreamGui &gui) : streamName(), gui(gui)
{
}
IQuery::~IQuery()
{
}
bool IQuery::draw()
{
	ImGui::InputText("Stream Name", &streamName);
	return ImGui::Button("Send");
}
// QueryText
QueryText::QueryText(TemStreamGui &gui) : IQuery(gui), text()
{
}
QueryText::QueryText(TemStreamGui &gui, String &&s) : IQuery(gui), text(std::move(s))
{
}
QueryText::~QueryText()
{
}
bool QueryText::draw()
{
	ImGui::InputTextMultiline("Text", &text);
	return IQuery::draw();
}
void QueryText::execute() const
{
	MessagePacket *packet = new MessagePacket();
	packet->message = TextMessage(text);
	packet->source.author = gui.getInfo().name;
	packet->source.destination = streamName;

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendSingleMessagePacket;
	e.user.data1 = reinterpret_cast<void *>(packet);
	if (SDL_PushEvent(&e) != 1)
	{
		logger->AddError("Failed to push SDL event: %s\n", SDL_GetError());
		delete packet;
	}
}
// Query Image
QueryImage::QueryImage(TemStreamGui &gui) : IQuery(gui), image()
{
}
QueryImage::QueryImage(TemStreamGui &gui, String &&s) : IQuery(gui), image(std::move(s))
{
}
QueryImage::~QueryImage()
{
}
bool QueryImage::draw()
{
	ImGui::InputText("Image path", &image);
	return IQuery::draw();
}
void QueryImage::execute() const
{
	std::thread thread(QueryImage::getPackets, image, MessageSource{gui.getInfo().name, streamName});
	thread.detach();
}
void QueryImage::getPackets(const String filename, const MessageSource source)
{
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		logger->AddError("Failed to open file: %s\n", filename.c_str());
		return;
	}

	MessagePackets *packets = new MessagePackets();

	{
		MessagePacket packet;
		packet.message = ImageMessage(true);
		packet.source = source;
		packets->push_back(std::move(packet));
	}
	{

		const Bytes bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		for (size_t i = 0; i < bytes.size(); i += KB(64))
		{
			MessagePacket packet;
			packet.message =
				Bytes(bytes.begin() + i, (i + KB(64)) > bytes.size() ? bytes.end() : (bytes.begin() + i + KB(64)));
			packet.source = source;
			packets->push_back(std::move(packet));
		}
	}
	{
		MessagePacket packet;
		packet.message = ImageMessage(false);
		packet.source = source;
		packets->push_back(std::move(packet));
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = reinterpret_cast<void *>(packets);
	if (SDL_PushEvent(&e) != 1)
	{
		logger->AddError("Failed to push SDL event: %s\n", SDL_GetError());
		delete packets;
	}
}
QueryAudio::QueryAudio(TemStreamGui &gui) : IQuery(gui)
{
}
QueryAudio::~QueryAudio()
{
}
bool QueryAudio::draw()
{
	return IQuery::draw();
}
void QueryAudio::execute() const
{
}
QueryVideo::QueryVideo(TemStreamGui &gui) : IQuery(gui)
{
}
QueryVideo::~QueryVideo()
{
}
bool QueryVideo::draw()
{
	return IQuery::draw();
}
void QueryVideo::execute() const
{
}
} // namespace TemStream