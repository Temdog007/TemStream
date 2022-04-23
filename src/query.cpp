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
Message::Source IQuery::getSource() const
{
	return Message::Source{gui.getInfo().name, streamName};
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
	Message::Source source;
	source.author = gui.getInfo().name;
	source.destination = streamName;

	if (!TemStreamGui::sendCreateMessage<Message::Text>(source))
	{
		return;
	}

	Message::Packet *packet = allocate<Message::Packet>();
	packet->payload.emplace<Message::Text>(text);
	packet->source = std::move(source);

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendSingleMessagePacket;
	e.user.data1 = packet;
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		deallocate(packet);
	}
}
// Query Image
QueryImage::QueryImage(TemStreamGui &gui) : IQuery(gui), image()
{
}
QueryImage::QueryImage(TemStreamGui &gui, const String &s) : IQuery(gui), image(s)
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
	Work::Task task(Work::SendImage(image, getSource()));
	(*gui).addWork(std::move(task));
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
			auto opt = Audio::getWindowsWithAudio();
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
	case Source::Window: {
		size_t i = 0;
		for (const auto &wp : windowNames)
		{
			snprintf(buffer, sizeof(buffer), "%s (%d)", wp.name.c_str(), wp.id);
			ImGui::RadioButton(buffer, &selected, i++);
		}
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
		const auto s = name == nullptr ? std::nullopt : std::make_optional<String>(name);
		Work::Task task(Work::StartRecording(getSource(), s, gui.getConfiguration().defaultSilenceThreshold / 100.f));
		(*gui).addWork(std::move(task));
	}
	break;
	case Source::Window: {
		const size_t index = static_cast<size_t>(selected);
		size_t i = 0;
		for (const auto &wp : windowNames)
		{
			if (i == index)
			{
				Work::Task task(Work::StartWindowRecording(getSource(), wp,
														   gui.getConfiguration().defaultSilenceThreshold / 100.f));
				(*gui).addWork(std::move(task));
				break;
			}
			++i;
		}
	}
	break;
	default:
		break;
	}
}
QueryVideo::QueryVideo(TemStreamGui &gui) : IQuery(gui), selection("/dev/video0")
{
}
QueryVideo::~QueryVideo()
{
}
bool QueryVideo::draw()
{
	static const char *selections[]{"Webcam", "Window"};
	int s = static_cast<int>(selection.index());
	if (ImGui::Combo("Source", &s, selections, IM_ARRAYSIZE(selections)))
	{
		switch (s)
		{
		case variant_index<VideoSelection, String>():
			selection.emplace<String>("/dev/video0");
			break;
		case variant_index<VideoSelection, WindowSelection>():
			selection.emplace<WindowSelection>(WindowSelection{Video::getRecordableWindows(), 0});
			break;
		default:
			break;
		}
	}
	struct Foo
	{
		void operator()(WindowSelection &ws)
		{
			char buffer[KB(1)];
			size_t i = 0;
			for (const auto &wp : ws.windows)
			{
				snprintf(buffer, sizeof(buffer), "%s (%d)", wp.name.c_str(), wp.windowId);
				ImGui::RadioButton(buffer, &ws.selected, i++);
			}
		}
		void operator()(String &s)
		{
			ImGui::InputText("Webcam location", &s);
		}
	};
	std::visit(Foo(), selection);
	return IQuery::draw();
}
void QueryVideo::execute() const
{
	struct Foo
	{
		void operator()(const WindowSelection &)
		{
			(*logger)(Logger::Error) << "Window not implemented..." << std::endl;
		}
		void operator()(const String &)
		{
			(*logger)(Logger::Error) << "Webcam not implemented..." << std::endl;
		}
	};
	std::visit(Foo(), selection);
}
} // namespace TemStream