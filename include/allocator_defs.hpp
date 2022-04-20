#pragma once

#include <main.hpp>

namespace TemStream
{
#if USE_CUSTOM_ALLOCATOR
using String = std::basic_string<char, std::char_traits<char>, Allocator<char>>;
using String32 = std::basic_string<char32_t, std::char_traits<char32_t>, Allocator<char32_t>>;
using StringStream = std::basic_ostringstream<char, std::char_traits<char>, Allocator<char>>;
template <typename T> using List = std::vector<T, Allocator<T>>;
template <typename T> using Deque = std::deque<T, Allocator<T>>;
template <typename K, typename V>
using Map = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, Allocator<std::pair<const K, V>>>;
} // namespace TemStream
namespace std
{
template <> struct hash<TemStream::String>
{
	std::size_t operator()(const TemStream::String &s) const
	{
		std::size_t value = s.size();
		for (auto c : s)
		{
			TemStream::hash_combine(value, c);
		}
		return value;
	}
};
} // namespace std
namespace ImGui
{
IMGUI_API bool InputText(const char *, TemStream::String *, ImGuiInputTextFlags flags = 0,
						 ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
IMGUI_API bool InputTextMultiline(const char *, TemStream::String *, const ImVec2 &size = ImVec2(0, 0),
								  ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr,
								  void *user_data = nullptr);
} // namespace ImGui
namespace TemStream
{
#else

using String = std::string;
using String32 = std::u32string;
using StringStream = std::ostringstream;
template <typename T> using List = std::vector<T>;
template <typename T> using Deque = std::deque<T>;
template <typename K, typename V> using Map = std::unordered_map<K, V>;
#endif

extern String &trim(String &);
extern String &ltrim(String &);
extern String &rtrim(String &);

using Bytes = List<char>;

template <typename T, typename... Args> T *allocate(Args &&...args)
{
	return new T(std::forward<Args>(args)...);
}
template <typename T> void deallocate(T *const t)
{
	delete t;
}

template <typename T> struct Deleter
{
	constexpr Deleter() noexcept = default;
	template <typename U, typename = std::_Require<std::is_convertible<U *, T *>>> Deleter(const Deleter<U> &) noexcept
	{
	}
	void operator()(T *t) const
	{
		deallocate(t);
	}
};

template <typename T> using unique_ptr = std::unique_ptr<T, Deleter<T>>;

template <typename T> using shared_ptr = std::shared_ptr<T>;

template <typename T, typename... Args> static inline unique_ptr<T> tem_unique(Args &&...args)
{
	return unique_ptr<T>(allocate<T>(std::forward<Args>(args)...), Deleter<T>());
}

template <typename T, typename... Args> static inline shared_ptr<T> tem_shared(Args &&...args)
{
	return shared_ptr<T>(allocate<T>(std::forward<Args>(args)...), Deleter<T>());
}
} // namespace TemStream