#include <main.hpp>

namespace TemStream
{
WorkPool WorkPool::workPool;
WorkPool::WorkPool() : threads(), workList(), ready(0)
{
}
WorkPool::~WorkPool()
{
	waitForAll();
}
void WorkPool::addWork(std::function<void()> &&f)
{
	using namespace std::chrono_literals;
	if (ready == 0)
	{
		threads.emplace_back([this]() { WorkPool::handleWork(*this); });
	}
	workList.push(std::move(f));
}
void WorkPool::waitForAll()
{
	for (auto &t : threads)
	{
		t.join();
	}
	threads.clear();
	workList.clear();
}
WorkPool::Readiness::Readiness(std::atomic_int32_t &ready) : ready(ready)
{
	++ready;
}
WorkPool::Readiness::~Readiness()
{
	--ready;
}
void WorkPool::handleWork(WorkPool &p)
{
	using namespace std::chrono_literals;

	std::optional<std::function<void()>> work;
	while (!appDone)
	{
		{
			Readiness r(p.ready);
			work = p.workList.pop(500ms);
			if (!work)
			{
				continue;
			}
		}

		(*work)();
	}
}
namespace Work
{
void checkFile(TemStreamGui &gui, String filename)
{
	try
	{
		IQuery *data = nullptr;
		if (isImage(filename.c_str()))
		{
			data = allocateAndConstruct<QueryImage>(gui, filename);
			goto end;
		}

#if TEMSTREAM_USE_OPENCV
		if (cv::VideoCapture(cv::String(filename)).isOpened())
		{
			data = allocateAndConstruct<QueryVideo>(gui, filename);
			goto end;
		}
#endif
		{
			std::ifstream file(filename.c_str());
			String s((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			data = allocateAndConstruct<QueryText>(gui, std::move(s));
			goto end;
		}
	end:
		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::SetQueryData;
		e.user.data1 = data;
		if (!tryPushEvent(e))
		{
			destroyAndDeallocate(data);
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Failed to check file: " << filename << std::endl;
	}
}
void sendImage(String filename, Message::Source source)
{
	std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		(*logger)(Logger::Error) << "Failed to open file: " << filename << std::endl;
		return;
	}

	if (!TemStreamGui::sendCreateMessage<Message::Image>(source))
	{
		return;
	}

	MessagePackets *packets = allocateAndConstruct<MessagePackets>();
	Message::prepareImageBytes(file, source,
							   [packets](Message::Packet &&packet) { packets->emplace_back(std::move(packet)); });
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = reinterpret_cast<void *>(packets);
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		destroyAndDeallocate(packets);
	}
}
void loadSurface(Message::Source source, ByteList bytes)
{
	(*logger)(Logger::Trace) << "Loading image data: " << bytes.size() / KB(1) << "KB" << std::endl;
	SDL_RWops *src = SDL_RWFromConstMem(bytes.data(), bytes.size());
	if (src == nullptr)
	{
		logSDLError("Failed to load image data");
		return;
	}
	SDL_Surface *surface = IMG_Load_RW(src, 0);
	if (surface == nullptr)
	{
		(*logger)(Logger::Error) << "Surface load error: " << IMG_GetError() << std::endl;
		return;
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SetSurfaceToStreamDisplay;
	e.user.data1 = surface;
	auto ptr = allocateAndConstruct<Message::Source>(source);
	e.user.data2 = ptr;
	if (!tryPushEvent(e))
	{
		SDL_FreeSurface(surface);
		destroyAndDeallocate(ptr);
	}
}

void startPlayback(Message::Source source, const std::optional<String> name, const float volume)
{
	auto ptr = Audio::startPlayback(source, name.has_value() ? name->c_str() : nullptr, volume);
	if (ptr == nullptr)
	{
		return;
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::AddAudio;
	e.user.data1 = ptr.get();
	if (tryPushEvent(e))
	{
		ptr.release();
	}

	// Pointer will deleted if not released
}

void startRecordingAudio(const Message::Source source, const std::optional<String> name, const float silenceThreshold)
{
	auto ptr = Audio::startRecording(source, name.has_value() ? name->c_str() : nullptr, silenceThreshold);
	if (ptr == nullptr)
	{
		return;
	}

	if (!TemStreamGui::sendCreateMessage<Message::Audio>(source))
	{
		return;
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::AddAudio;
	e.user.data1 = ptr.get();
	if (tryPushEvent(e))
	{
		ptr.release();
	}

	// Pointer will deleted if not released
}
void startRecordingWindowAudio(const Message::Source source, const WindowProcess windowProcess,
							   const float silenceThreshold)
{
	auto ptr = Audio::startRecordingWindow(source, windowProcess, silenceThreshold);
	if (ptr == nullptr)
	{
		return;
	}

	if (!TemStreamGui::sendCreateMessage<Message::Audio>(source))
	{
		return;
	}

	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::AddAudio;
	e.user.data1 = ptr.get();
	if (tryPushEvent(e))
	{
		ptr.release();
	}

	// Pointer will deleted if not released
}
} // namespace Work
} // namespace TemStream
