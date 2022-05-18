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
	return strcmp(getExtension(filename), "ttf") == 0;
}
bool isJpeg(const char *filename)
{
	const char *ext = getExtension(filename);
	return strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0;
}
bool isXPM(const char *filename)
{
	return strcmp(getExtension(filename), "xpm") == 0;
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
	ImGui::SetNextWindowSize(ImVec2(w / 4.f, w / 4.f), ImGuiCond_FirstUseEver);
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
	(*logger)(Logger::Level::Error) << str << ": " << SDL_GetError() << std::endl;
}
} // namespace TemStream

#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
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
#endif