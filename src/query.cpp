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
		perror("fopen");
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
	packet.source.author = gui.getInfo().name;
	packet.source.destination = streamName;
	packets.push_back(std::move(packet));

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

	{
		MessagePacket packet;
		packet.message = ImageMessage(true);
		packet.source.author = gui.getInfo().name;
		packet.source.destination = streamName;
		packets.push_back(std::move(packet));
	}
	{
		std::ifstream file(image, std::ios::in | std::ios::binary);

		const Bytes bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		for (size_t i = 0; i < bytes.size(); i += KB(64))
		{
			MessagePacket packet;
			packet.message =
				Bytes(bytes.begin() + i, (i + KB(64)) > bytes.size() ? bytes.end() : (bytes.begin() + i + KB(64)));
			packet.source.author = gui.getInfo().name;
			packet.source.destination = streamName;
			packets.push_back(std::move(packet));
		}
	}
	{
		MessagePacket packet;
		packet.message = ImageMessage(false);
		packet.source.author = gui.getInfo().name;
		packet.source.destination = streamName;
		packets.push_back(std::move(packet));
	}
	return packets;
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