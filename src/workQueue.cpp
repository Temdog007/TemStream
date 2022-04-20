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
SendImage::SendImage(const String &filename, const MessageSource &source) : filename(filename), source(source)
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
LoadSurface::LoadSurface(const MessageSource &source, Bytes &&bytes) : source(source), bytes(std::move(bytes))
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
	auto ptr = allocate<MessageSource>(source);
	e.user.data2 = ptr;
	if (!tryPushEvent(e))
	{
		SDL_FreeSurface(surface);
		deallocate(ptr);
	}
}
StartPlayback::StartPlayback(const MessageSource &source, const std::optional<String> &name)
	: source(source), name(name)
{
}
StartPlayback::~StartPlayback()
{
}
void StartPlayback::run() const
{
	auto ptr = Audio::startPlayback(source, name.has_value() ? name->c_str() : nullptr);
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
StartRecording::StartRecording(const MessageSource &source, const std::optional<String> &name)
	: source(source), name(name)
{
}
StartRecording::~StartRecording()
{
}
void StartRecording::run() const
{
	auto ptr = Audio::startRecording(source, name.has_value() ? name->c_str() : nullptr);
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
StartWindowRecording::StartWindowRecording(const MessageSource &source, const WindowProcess &wp)
	: source(source), windowProcess(wp)
{
}
StartWindowRecording::~StartWindowRecording()
{
}
void StartWindowRecording::run() const
{
	auto ptr = Audio::startRecordingWindow(source, windowProcess);
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
