#include <main.hpp>

namespace TemStream
{
WorkPool WorkPool::workPool;
WorkPool::WorkPool() : workList()
{
}
WorkPool::~WorkPool()
{
	clear();
}
void WorkPool::addWork(std::function<bool()> &&f)
{
	workList.push(std::move(f));
}
void WorkPool::clear()
{
	workList.clear();
}
void WorkPool::handleWorkInAnotherThread()
{
	for (size_t i = 0, n = std::thread::hardware_concurrency(); i < n; ++i)
	{
		std::thread thread([]() {
			using namespace std::chrono_literals;
			while (!appDone)
			{
				WorkPool::workPool.handleWork(500ms);
			}
		});
		thread.detach();
	}
}
namespace Work
{
bool fileIsBinary(const String &filename)
{
	const int check = 8000;
	const char nulChar = '\0';
	int i = 0;
	int count = 0;
	std::ifstream file(filename.c_str());
	for (auto iter = std::istreambuf_iterator<char>(file), end = std::istreambuf_iterator<char>();
		 iter != end && i < check; ++iter, ++i)
	{
		const char c = *iter;
		if (c == nulChar)
		{
			++count;
			if (count > 1)
			{
				return true;
			}
		}
	}
	return false;
}
void checkFile(TemStreamGui &gui, const String &filename)
{
	try
	{
		IQuery *data = nullptr;
		if (fileIsBinary(filename))
		{
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
		}
		else
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
void sendImage(const String &filename, const Message::Source &source)
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
void loadSurface(const Message::Source &source, const ByteList &bytes)
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

void startRecordingAudio(const Message::Source &source, const std::optional<String> &name, const float silenceThreshold)
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
void startRecordingWindowAudio(const Message::Source &source, const WindowProcess &windowProcess,
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
