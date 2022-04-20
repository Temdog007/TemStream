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
MessageSource IQuery::getSource() const
{
	return MessageSource{gui.getInfo().name, streamName};
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
	MessagePacket *packet = allocate<MessagePacket>();
	packet->message = TextMessage(text);
	packet->source.author = gui.getInfo().name;
	packet->source.destination = streamName;

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendSingleMessagePacket;
	e.user.data1 = reinterpret_cast<void *>(packet);
	if (!tryPushEvent(e))
	{
		deallocate(packet);
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
	std::thread thread(QueryImage::getPackets, image, getSource());
	thread.detach();
}
void QueryImage::getPackets(const String filename, const MessageSource source)
{
	std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		(*logger)(Logger::Error) << "Failed to open file: " << filename << std::endl;
		return;
	}

	MessagePackets *packets = allocate<MessagePackets>();
	const Bytes bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	{
		MessagePacket packet;
		packet.message = ImageMessage(static_cast<uint64_t>(bytes.size()));
		packet.source = source;
		packets->push_back(std::move(packet));
	}
	{

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
		packet.message = ImageMessage(std::monostate{});
		packet.source = source;
		packets->push_back(std::move(packet));
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = reinterpret_cast<void *>(packets);
	if (!tryPushEvent(e))
	{
		deallocate(packets);
	}
}
QueryAudio::QueryAudio(TemStreamGui &gui) : IQuery(gui), windowNames(), source(Source::Device), selected(-1)
{
}
QueryAudio::~QueryAudio()
{
}
bool QueryAudio::draw()
{
	static const char *selection[]{"Device", "Window"};
	int s = static_cast<int>(source);
	if (ImGui::Combo("Source", &s, selection, IM_ARRAYSIZE(selection)))
	{
		source = static_cast<Source>(s);
		if (source == Source::Window)
		{
			auto opt = Audio::getListOfWindowsWithAudio();
			if (opt.has_value())
			{
				windowNames = std::move(*opt);
			}
		}
	}
	char buffer[KB(1)];
	switch (source)
	{
	case Source::Device: {
		const int count = SDL_GetNumAudioDevices(SDL_TRUE);
		for (int i = 0; i < count; ++i)
		{
			snprintf(buffer, sizeof(buffer), "%s", SDL_GetAudioDeviceName(i, SDL_TRUE));
			ImGui::RadioButton(buffer, &selected, i);
		}
	}
	break;
	case Source::Window:
		for (size_t i = 0; i < windowNames.size(); ++i)
		{
			const auto &wp = windowNames[i];
			snprintf(buffer, sizeof(buffer), "%s (%d)", wp.name.c_str(), wp.id);
			ImGui::RadioButton(buffer, &selected, i);
		}
		break;
	default:
		return false;
	}

	return IQuery::draw();
}
void QueryAudio::execute() const
{
	switch (source)
	{
	case Source::Device: {
		const int count = SDL_GetNumAudioDevices(SDL_TRUE);
		if (selected < 0 || selected >= count)
		{
			(*logger)(Logger::Error) << "Invalid audio device selected" << std::endl;
			return;
		}
		const char *name = SDL_GetAudioDeviceName(selected, SDL_TRUE);
		auto ptr = Audio::startRecording(getSource(), name);
		if (ptr)
		{
			gui.addAudio(std::move(ptr));
		}
	}
	break;
	case Source::Window: {
		const size_t index = static_cast<size_t>(selected);
		if (index >= windowNames.size())
		{
			break;
		}
		auto ptr = Audio::startRecordingWindow(getSource(), windowNames[index]);
		if (ptr)
		{
			gui.addAudio(std::move(ptr));
		}
	}
	break;
	default:
		break;
	}
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