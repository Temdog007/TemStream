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

	Message::Packet *packet = allocateAndConstruct<Message::Packet>();
	packet->payload.emplace<Message::Text>(text);
	packet->source = std::move(source);

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendSingleMessagePacket;
	e.user.data1 = packet;
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(packet);
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
	Task::addTask(std::async(TaskPolicy, Task::sendImage, image, getSource()));
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
		Task::addTask(std::async(TaskPolicy, Task::startRecordingAudio, getSource(), s,
								 gui.getConfiguration().defaultSilenceThreshold / 100.f));
	}
	break;
	case Source::Window: {
		const size_t index = static_cast<size_t>(selected);
		size_t i = 0;
		for (const auto &wp : windowNames)
		{
			if (i == index)
			{
				Task::addTask(std::async(TaskPolicy, Task::startRecordingWindowAudio, getSource(), wp,
										 gui.getConfiguration().defaultSilenceThreshold / 100.f));
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
QueryVideo::QueryVideo(TemStreamGui &gui) : IQuery(gui), selection(WebCamSelection())
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
		case variant_index<VideoSelection, WebCamSelection>():
			selection.emplace<WebCamSelection>(WebCamSelection{Video::FrameData(), 0, 100});
			break;
		case variant_index<VideoSelection, WindowSelection>():
			selection.emplace<WindowSelection>(
				WindowSelection{Video::FrameData(), Video::getRecordableWindows(), 100, 0});
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
			ws.frameData.draw();
			ImGui::SliderInt("Scale", &ws.scale, 1, 100);
		}
		void operator()(WebCamSelection &w)
		{
			ImGui::InputInt("Index", &w.index, 1, 10);
			w.frameData.draw();
			ImGui::SliderInt("Scale", &w.scale, 1, 100);
		}
	};
	std::visit(Foo(), selection);
	return IQuery::draw();
}
void Video::FrameData::draw()
{
	if (ImGui::InputInt("Frames per second", &fps, 1, 120))
	{
		fps = std::clamp(fps, 1, 120);
	}
	if (ImGui::InputInt("Bitrate in Mbps", &bitrateInMbps, 1, 100))
	{
		bitrateInMbps = std::clamp(bitrateInMbps, 1, 100);
	}

#if !TEMSTREAM_USE_OPENH264
	if (ImGui::InputInt("Key Frame Interval", &keyFrameInterval, 1, 1000))
	{
		keyFrameInterval = std::clamp(keyFrameInterval, 1, 100);
	}
#endif
}
void QueryVideo::execute() const
{
	struct Foo
	{
		Message::Source source;
		TemStreamGui &gui;
		void operator()(const WindowSelection &ws) const
		{
			int i = 0;
			for (const auto &wp : ws.windows)
			{
				if (i == ws.selected)
				{
					auto ptr = Video::recordWindow(wp, source, ws.scale, ws.frameData);
					if (ptr == nullptr)
					{
						(*logger)(Logger::Error) << "Failed to start recording window " << wp.name << std::endl;
					}
					else
					{
						gui.addVideo(std::move(ptr));
					}
					break;
				}
				++i;
			}
		}
		void operator()(const WebCamSelection wb) const
		{
			auto ptr = Video::recordWebcam(wb.index, source, wb.scale, wb.frameData);
			if (ptr == nullptr)
			{
				(*logger)(Logger::Error) << "Failed to start recording webcam " << wb.index << std::endl;
			}
			else
			{
				gui.addVideo(std::move(ptr));
			}
		}
	};
	std::visit(Foo{getSource(), gui}, selection);
}
} // namespace TemStream