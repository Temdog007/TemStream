/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include <main.hpp>

namespace TemStream
{
shared_ptr<WorkPool> WorkPool::globalWorkPool = nullptr;
WorkPool::WorkPool() : workList()
{
}
WorkPool::~WorkPool()
{
	clear();
}
void WorkPool::add(std::function<bool()> &&f)
{
	workList.push(std::move(f));
}
void WorkPool::addWork(std::function<bool()> &&f)
{
	globalWorkPool->add(std::move(f));
}
void WorkPool::clear()
{
	workList.clear();
}
void WorkPool::setGlobalWorkPool(shared_ptr<WorkPool> work)
{
	globalWorkPool = work;
}
List<std::thread> WorkPool::handleWorkAsync()
{
	List<std::thread> threads;
	for (size_t i = 0, n = std::thread::hardware_concurrency(); i < n; ++i)
	{
		std::thread thread([]() {
			using namespace std::chrono_literals;
			while (!appDone)
			{
				globalWorkPool->handleWork(500ms);
				std::this_thread::sleep_for(1ms);
			}
		});
		threads.push_back(std::move(thread));
	}
	return threads;
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
void checkFile(TemStreamGui &gui, const Message::Source &source, const String &filename)
{
	try
	{
		IQuery *data = nullptr;
		if (isJpeg(filename.c_str()) || isXPM(filename.c_str()))
		{
			data = allocateAndConstruct<QueryImage>(gui, source, filename);
		}
		else if (fileIsBinary(filename))
		{
			if (isImage(filename.c_str()))
			{
				data = allocateAndConstruct<QueryImage>(gui, source, filename);
			}
			else if (cv::VideoCapture(cv::String(filename)).isOpened())
			{
				data = allocateAndConstruct<QueryVideo>(gui, source, filename);
			}
		}
		else
		{
			std::ifstream file(filename.c_str());
			String s((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			data = allocateAndConstruct<QueryText>(gui, source, std::move(s));
		}
		SDL_Event e;
		e.type = SDL_USEREVENT;
		e.user.code = TemStreamEvent::SetQueryData;
		e.user.data1 = data;
		if (!tryPushEvent(e))
		{
			destroyAndDeallocate(data);
		}
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Failed to check file: " << filename << "; " << e.what() << std::endl;
	}
}
void sendImage(const String &filename, const Message::Source &source)
{
	std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
	if (!file.is_open())
	{
		(*logger)(Logger::Level::Error) << "Failed to open file: " << filename << std::endl;
		return;
	}

	MessagePackets *packets = allocateAndConstruct<MessagePackets>();
	Message::prepareLargeBytes(file, [packets, &source](Message::LargeFile &&largeFile) {
		Message::Packet packet;
		packet.source = source;
		Message::Image image{std::move(largeFile)};
		packet.payload.emplace<Message::Image>(std::move(image));
		packets->emplace_back(std::move(packet));
	});
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
	(*logger)(Logger::Level::Trace) << "Loading image data: " << bytes.size() / KB(1) << "KB" << std::endl;

	SDL_RWops *src = SDL_RWFromConstMem(bytes.data(), bytes.size());
	if (src == nullptr)
	{
		logSDLError("Failed to load image data");
		return;
	}
	SDL_Surface *surface = IMG_Load_RW(src, 0);
	if (surface == nullptr)
	{
		(*logger)(Logger::Level::Error) << "Surface load error: " << IMG_GetError() << std::endl;
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
	auto ptr = AudioSource::startRecording(source, name.has_value() ? name->c_str() : nullptr, silenceThreshold);
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
void startRecordingWindowAudio(const Message::Source &source, const WindowProcess &windowProcess,
							   const float silenceThreshold)
{
	auto ptr = AudioSource::startRecordingWindow(source, windowProcess, silenceThreshold);
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
} // namespace TemStream
