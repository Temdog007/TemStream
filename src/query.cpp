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
	WorkPool::workPool.addWork([image = image, source = getSource()]() {
		Work::sendImage(image, source);
		return false;
	});
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
		WorkPool::workPool.addWork(
			[s, source = getSource(), silence = gui.getConfiguration().defaultSilenceThreshold]() {
				Work::startRecordingAudio(source, s, silence / 100.f);
				return false;
			});
	}
	break;
	case Source::Window: {
		const size_t index = static_cast<size_t>(selected);
		size_t i = 0;
		for (const auto &wp : windowNames)
		{
			if (i == index)
			{
				WorkPool::workPool.addWork(
					[wp, source = getSource(), silence = gui.getConfiguration().defaultSilenceThreshold]() {
						Work::startRecordingWindowAudio(source, wp, silence / 100.f);
						return false;
					});
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
QueryVideo::QueryVideo(TemStreamGui &gui) : IQuery(gui), selection(WebCamSelection{Video::FrameData(), 0, 100})
{
}
QueryVideo::QueryVideo(TemStreamGui &gui, const String &s)
	: IQuery(gui), selection(WebCamSelection{Video::FrameData(), s, 100})
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
			if (ImGui::InputInt("Scale (%)", &ws.scale))
			{
				ws.scale = std::clamp(ws.scale, 1, 100);
			}
		}
		void operator()(WebCamSelection &w)
		{
			struct Bar
			{
				bool operator()(String &s)
				{
					return ImGui::InputText("Filename", &s);
				}
				bool operator()(int32_t &index)
				{
					return ImGui::InputInt("Index", &index, 1, 10);
				}
			};
			static const char *selections[]{"Index", "Filename"};
			int selected = w.arg.index();
			if (ImGui::Combo("Type", &selected, selections, IM_ARRAYSIZE(selections)))
			{
				switch (selected)
				{
				case variant_index<VideoCaptureArg, int32_t>():
					w.arg.emplace<int32_t>(0);
					break;
				case variant_index<VideoCaptureArg, String>():
					w.arg.emplace<String>("/dev/video0");
					break;
				default:
					break;
				}
			}
			std::visit(Bar(), w.arg);
			w.frameData.draw();
			if (ImGui::InputInt("Scale (%)", &w.scale))
			{
				w.scale = std::clamp(w.scale, 1, 100);
			}
		}
	};
	std::visit(Foo(), selection);
	return IQuery::draw();
}
std::ostream &operator<<(std::ostream &os, const VideoCaptureArg &arg)
{
	struct Foo
	{
		std::ostream &os;
		std::ostream &operator()(const String &s)
		{
			os << s;
			return os;
		}
		std::ostream &operator()(const int32_t index)
		{
			os << index;
			return os;
		}
	};
	return std::visit(Foo{os}, arg);
}
void Video::FrameData::draw()
{
	if (ImGui::InputInt("Frames per second", &fps, 1, 120))
	{
		fps = std::clamp(fps, 1, 120);
	}
	ImGui::Checkbox("Use MJPEG", &jpegCapture);
	if (!jpegCapture)
	{
		if (ImGui::InputInt("Bitrate in Mbps", &bitrateInMbps, 1))
		{
			bitrateInMbps = std::clamp(bitrateInMbps, 1, 100);
		}

		if (ImGui::InputInt("Key Frame Interval", &keyFrameInterval, 1))
		{
			keyFrameInterval = std::clamp(keyFrameInterval, 1, fps * 30);
		}
	}
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
						gui.addVideo(ptr);
					}
					break;
				}
				++i;
			}
		}
		void operator()(const WebCamSelection wb) const
		{
			auto ptr = Video::recordWebcam(wb.arg, source, wb.scale, wb.frameData);
			if (ptr == nullptr)
			{
				(*logger)(Logger::Error) << "Failed to start recording webcam " << wb.arg << std::endl;
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