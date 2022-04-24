#include <main.hpp>

namespace TemStream
{
const std::launch TaskPolicy = std::launch::async;
WorkList Task::workList;
void Task::addTask(std::future<void> &&f)
{
	workList.push_back(std::move(f));
}
void Task::cleanupTasks()
{
	using namespace std::chrono_literals;
	for (auto iter = workList.begin(); iter != workList.end();)
	{
		if (iter->wait_for(0ms) == std::future_status::ready)
		{
			iter = workList.erase(iter);
		}
		else
		{
			++iter;
		}
	}
}
void Task::waitForAll()
{
	for (auto &f : workList)
	{
		f.wait();
	}
	workList.clear();
}
void Task::checkFile(TemStreamGui &gui, String filename)
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
void Task::sendImage(String filename, Message::Source source)
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
void Task::loadSurface(Message::Source source, Bytes bytes)
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

void Task::startPlayback(Message::Source source, const std::optional<String> name, const float volume)
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

void Task::startRecordingAudio(const Message::Source source, const std::optional<String> name,
							   const float silenceThreshold)
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
void Task::startRecordingWindowAudio(const Message::Source source, const WindowProcess windowProcess,
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
} // namespace TemStream
