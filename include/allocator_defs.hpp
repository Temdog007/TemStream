#pragma once

#include <main.hpp>

#if USE_CUSTOM_ALLOCATOR
namespace TemStream
{
using String = std::basic_string<char, std::char_traits<char>, Allocator<char>>;
using String32 = std::basic_string<char32_t, std::char_traits<char32_t>, Allocator<char32_t>>;
using StringStream = std::basic_ostringstream<char, std::char_traits<char>, Allocator<char>>;
template <typename T> using List = std::vector<T, Allocator<T>>;
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
IMGUI_API bool InputText(const char *label, TemStream::String *str, ImGuiInputTextFlags flags = 0,
						 ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
}
#else
namespace TemStream
{
using String = std::string;
using String32 = std::u32string;
using StringStream = std::ostringstream;
template <typename T> using List = std::vector<T>;
template <typename T> using Deque = std::deque<T>;
template <typename K, typename V> using Map = std::unordered_map<K, V>;
} // namespace TemStream
#endif

using Bytes = TemStream::List<char>;