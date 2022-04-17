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
bool IQuery::handleDropFile(const char *)
{
	return false;
}
bool IQuery::handleDropText(const char *)
{
	return false;
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
QueryText::~QueryText()
{
}
bool QueryText::draw()
{
	ImGui::InputText("Text", &text);
	return IQuery::draw();
}
bool QueryText::handleDropText(const char *c)
{
	text = c;
	return false;
}
bool QueryText::handleDropFile(const char *c)
{
	FILE *file = fopen(c, "r");
	if (file == nullptr)
	{
		return false;
	}
	char ch;
	text.clear();
	while ((ch = fgetc(file)) != EOF)
	{
		text += ch;
	}
	fclose(file);
	return false;
}
MessagePackets QueryText::getPackets() const
{
	MessagePackets packets;

	MessagePacket packet;
	packet.message = TextMessage(text);
	packet.source.destination = streamName;
	packets.emplace_back(std::move(packet));

	return packets;
}
// Query Image
QueryImage::QueryImage(TemStreamGui &gui) : IQuery(gui), image()
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
bool QueryImage::handleDropFile(const char *c)
{
	image = c;
	return false;
}
MessagePackets QueryImage::getPackets() const
{
	MessagePackets packets;

	FILE *file = fopen(image.c_str(), "rb");
	if (file == nullptr)
	{
		perror("fopen");
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "File not found", "File not found", gui.window);
		goto end;
	}
	{
		MessagePacket packet;
		packet.message = ImageMessage(true);
		packet.source.destination = streamName;
		packets.emplace_back(std::move(packet));
	}
	while (readChunkFromFile(file, packets))
		;
	{
		MessagePacket packet;
		packet.message = ImageMessage(false);
		packet.source.destination = streamName;
		packets.emplace_back(std::move(packet));
	}
	fclose(file);
end:
	return packets;
}
bool QueryImage::readChunkFromFile(FILE *file, MessagePackets &packets) const
{
	Bytes bytes;
	char ch;
	while ((ch = fgetc(file)) != EOF)
	{
		bytes.push_back(ch);
		if (bytes.size() > KB(64))
		{
			MessagePacket packet;
			packet.source.destination = streamName;
			packet.message = ImageMessage(std::move(bytes));
			packets.emplace_back(std::move(packet));
			return true;
		}
	}
	if (!bytes.empty())
	{
		MessagePacket packet;
		packet.source.destination = streamName;
		packet.message = ImageMessage(std::move(bytes));
		packets.emplace_back(std::move(packet));
	}
	return false;
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
MessagePackets QueryAudio::getPackets() const
{
	return MessagePackets();
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
MessagePackets QueryVideo::getPackets() const
{
	return MessagePackets();
}
} // namespace TemStream