#include <main.hpp>

namespace TemStream
{
Map<std::thread::id, size_t> LogMutex::threads;
LogMutex::LogMutex(Mutex &m, const char *name) : m(m), name(name), id(0)
{
	const auto tid = std::this_thread::get_id();
	auto iter = threads.find(tid);
	if (iter == threads.end())
	{
		id = threads.size();
		threads.emplace(tid, id);
	}
	else
	{
		id = iter->second;
	}
	std::cout << "-> " << id << ": " << name << std::endl;
	m.lock();
}
LogMutex::~LogMutex()
{
	m.unlock();
	std::cout << "<- " << id << ": " << name << std::endl;
}
bool openSocket(int &fd, const char *hostname, const char *port, const bool isServer)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	AddrInfo info;
	if (!info.getInfo(hostname, port, hints))
	{
		return false;
	}

	if (!info.makeSocket(fd) || fd < 0)
	{
		perror("socket");
		return false;
	}

	if (isServer)
	{
		if (!info.bind(fd))
		{
			perror("bind");
			return false;
		}
		if (listen(fd, 10) < 0)
		{
			perror("listen");
			return false;
		}
	}
	else if (!info.connect(fd))
	{
		perror("connect");
		return false;
	}

	return true;
}

PollState pollSocket(const int fd, const int timeout)
{
	struct pollfd inputfd;
	inputfd.fd = fd;
	inputfd.events = POLLIN;
	inputfd.revents = 0;
	switch (poll(&inputfd, 1, timeout))
	{
	case -1:
		perror("poll");
		return PollState::Error;
	case 0:
		return PollState::NoData;
	default:
		return (inputfd.revents & POLLIN) == 0 ? PollState::NoData : PollState::GotData;
	}
}

SDL_MutexWrapper::SDL_MutexWrapper() : mutex(SDL_CreateMutex())
{
	if (mutex == nullptr)
	{
		fprintf(stderr, "Failed to create mutex: %s\n", SDL_GetError());
	}
}
SDL_MutexWrapper::~SDL_MutexWrapper()
{
	puts("Deleted mutex");
	SDL_DestroyMutex(mutex);
	mutex = nullptr;
}
void SDL_MutexWrapper::lock()
{
	SDL_LockMutex(mutex);
}
void SDL_MutexWrapper::unlock()
{
	SDL_UnlockMutex(mutex);
}
} // namespace TemStream

struct InputTextCallback_UserData
{
	TemStream::String *Str;
	ImGuiInputTextCallback ChainCallback;
	void *ChainCallbackUserData;
};

static int InputTextCallback(ImGuiInputTextCallbackData *data)
{
	InputTextCallback_UserData *user_data = (InputTextCallback_UserData *)data->UserData;
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
	{
		// Resize string callback
		// If for some reason we refuse the new length (BufTextLen) and/or capacity (BufSize) we need to set them back
		// to what we want.
		auto *str = user_data->Str;
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(data->BufTextLen);
		data->Buf = (char *)str->c_str();
	}
	else if (user_data->ChainCallback)
	{
		// Forward to user callback, if any
		data->UserData = user_data->ChainCallbackUserData;
		return user_data->ChainCallback(data);
	}
	return 0;
}
bool ImGui::InputText(const char *label, TemStream::String *str, ImGuiInputTextFlags flags,
					  ImGuiInputTextCallback callback, void *user_data)
{
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str = str;
	cb_user_data.ChainCallback = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return ImGui::InputText(label, (char *)str->c_str(), str->capacity() + 1, flags, InputTextCallback, &cb_user_data);
}