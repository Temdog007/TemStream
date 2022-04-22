#include <main.hpp>

namespace TemStream
{
namespace Work
{
CheckFile::CheckFile(const String &filename, TemStreamGui &gui) : filename(filename), gui(gui)
{
}
CheckFile::~CheckFile()
{
}
void CheckFile::run() const
{
	IQuery *data = nullptr;
	if (isImage(filename.c_str()))
	{
		data = allocate<QueryImage>(gui, filename);
	}
	else
	{
		std::ifstream file(filename.c_str());
		String s((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		data = allocate<QueryText>(gui, std::move(s));
	}
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SetQueryData;
	e.user.data1 = data;
	if (!tryPushEvent(e))
	{
		deallocate(data);
	}
}
SendImage::SendImage(const String &filename, const Message::Source &source) : filename(filename), source(source)
{
}
SendImage::~SendImage()
{
}
void SendImage::run() const
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

	MessagePackets *packets = allocate<MessagePackets>();
	Message::prepareImageBytes(file, source,
							   [packets](Message::Packet &&packet) { packets->emplace_back(std::move(packet)); });
	SDL_Event e;
	e.type = SDL_USEREVENT;
	e.user.code = TemStreamEvent::SendMessagePackets;
	e.user.data1 = reinterpret_cast<void *>(packets);
	e.user.data2 = &e;
	if (!tryPushEvent(e))
	{
		deallocate(packets);
	}
}
LoadSurface::LoadSurface(const Message::Source &source, Bytes &&bytes) : source(source), bytes(std::move(bytes))
{
}
LoadSurface::~LoadSurface()
{
}
void LoadSurface::run() const
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
	auto ptr = allocate<Message::Source>(source);
	e.user.data2 = ptr;
	if (!tryPushEvent(e))
	{
		SDL_FreeSurface(surface);
		deallocate(ptr);
	}
}
StartPlayback::StartPlayback(const Message::Source &source, const std::optional<String> &name, const float volume)
	: source(source), name(name), volume(volume)
{
}
StartPlayback::~StartPlayback()
{
}
void StartPlayback::run() const
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
StartRecording::StartRecording(const Message::Source &source, const std::optional<String> &name,
							   const float silenceThreshold)
	: source(source), name(name), silenceThreshold(silenceThreshold)
{
}
StartRecording::~StartRecording()
{
}
void StartRecording::run() const
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
StartWindowRecording::StartWindowRecording(const Message::Source &source, const WindowProcess &wp,
										   const float silenceThreshold)
	: source(source), windowProcess(wp), silenceThreshold(silenceThreshold)
{
}
StartWindowRecording::~StartWindowRecording()
{
}
void StartWindowRecording::run() const
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
WorkQueue::WorkQueue() : tasks(), mutex()
{
}
WorkQueue::~WorkQueue()
{
}
void WorkQueue::addWork(Work::Task &&task)
{
	LOCK(mutex);
	tasks.emplace_back(std::move(task));
}
std::optional<Work::Task> WorkQueue::getWork()
{
	std::optional<Work::Task> rval = std::nullopt;
	if (mutex.try_lock())
	{
		if (!tasks.empty())
		{
			rval.emplace(std::move(tasks.front()));
			tasks.pop_front();
		}
		mutex.unlock();
	}
	return rval;
}
} // namespace TemStream
