#include <main.hpp>

namespace TemStream
{
SDL_MutexWrapper::SDL_MutexWrapper() : mutex(SDL_CreateMutex())
{
	if (mutex == nullptr)
	{
		logSDLError("Failed to create mutex");
	}
}
SDL_MutexWrapper::~SDL_MutexWrapper()
{
	logger->AddTrace("Deleted mutex");
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
bool isTTF(const char *filename)
{
	const size_t len = strlen(filename);
	const char *c = filename + (len - 1);
	while (c != filename && *c != '.')
	{
		--c;
	}
	return strcmp(c + 1, "ttf") == 0;
}
bool isImage(const char *filename)
{
	SDL_Surface *surface = IMG_Load(filename);
	const bool isImage = surface != nullptr;
	SDL_FreeSurface(surface);
	return isImage;
}
void SetWindowMinSize(SDL_Window *window)
{
	int w, h;
	SDL_GetWindowSize(window, &w, &h);
	ImGui::SetNextWindowSize(ImVec2(w / 4, w / 4), ImGuiCond_FirstUseEver);
}
bool tryPushEvent(SDL_Event &e)
{
	if (SDL_PushEvent(&e) != 1)
	{
		logSDLError("Failed to push SDL event");
		return false;
	}
	return true;
}
void logSDLError(const char *str)
{
	(*logger)(Logger::Error) << str << ": " << SDL_GetError() << std::endl;
}
} // namespace TemStream

#if USE_CUSTOM_ALLOCATOR
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
#endif

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

bool ImGui::InputTextMultiline(const char *label, TemStream::String *str, const ImVec2 &size, ImGuiInputTextFlags flags,
							   ImGuiInputTextCallback callback, void *user_data)
{
	IM_ASSERT((flags & ImGuiInputTextFlags_CallbackResize) == 0);
	flags |= ImGuiInputTextFlags_CallbackResize;

	InputTextCallback_UserData cb_user_data;
	cb_user_data.Str = str;
	cb_user_data.ChainCallback = callback;
	cb_user_data.ChainCallbackUserData = user_data;
	return InputTextMultiline(label, (char *)str->c_str(), str->capacity() + 1, size, flags, InputTextCallback,
							  &cb_user_data);
}