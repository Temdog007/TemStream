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

#pragma once

#include <main.hpp>

namespace TemStream
{
#if TEMSTREAM_USE_CUSTOM_ALLOCATOR
using String = std::basic_string<char, std::char_traits<char>, Allocator<char>>;
using String32 = std::basic_string<char32_t, std::char_traits<char32_t>, Allocator<char32_t>>;
using StringStream = std::basic_stringstream<char, std::char_traits<char>, Allocator<char>>;
template <typename T> using List = std::vector<T, Allocator<T>>;
template <typename T> using Deque = std::deque<T, Allocator<T>>;
template <typename K> using Set = std::unordered_set<K, std::hash<K>, std::equal_to<K>, Allocator<K>>;
template <typename K, typename V>
using Map = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, Allocator<std::pair<const K, V>>>;

template <typename T> using LinkedList = std::list<T, Allocator<T>>;
template <typename T> using Queue = std::queue<T, LinkedList<T>>;

using UTF8Converter = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t, Allocator<char32_t>, Allocator<char>>;

template <typename T, typename... Args> T *allocateAndConstruct(Args &&...args)
{
	Allocator<T> a;
	T *t = a.allocate(1);
	a.construct(t, std::forward<Args>(args)...);
	return t;
}
template <typename T> void destroyAndDeallocate(T *const t)
{
	Allocator<T> a;
	a.destroy(t);
	a.deallocate(t);
}

template <typename T> struct Deleter
{
	constexpr Deleter() noexcept = default;
#if __unix__
	template <typename U, typename = std::_Require<std::is_convertible<U *, T *>>>
	constexpr Deleter(Deleter<U>) noexcept
	{
	}
#else
	template <typename U> constexpr Deleter(Deleter<U>) noexcept
	{
	}
#endif
	void operator()(T *t) const
	{
		destroyAndDeallocate(t);
	}
};

template <typename T> using unique_ptr = std::unique_ptr<T, Deleter<T>>;

template <typename T> using shared_ptr = std::shared_ptr<T>;

template <typename T, typename... Args> static inline unique_ptr<T> tem_unique(Args &&...args)
{
	return unique_ptr<T>(allocateAndConstruct<T>(std::forward<Args>(args)...), Deleter<T>());
}

template <typename T, typename... Args> static inline shared_ptr<T> tem_shared(Args &&...args)
{
	return shared_ptr<T>(allocateAndConstruct<T>(std::forward<Args>(args)...), Deleter<T>());
}

template <typename T> static inline shared_ptr<T> tem_shared(T *t)
{
	return shared_ptr<T>(t, Deleter<T>());
}
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

#if TEMSTREAM_HAS_GUI
namespace ImGui
{
IMGUI_API bool InputText(const char *, TemStream::String *, ImGuiInputTextFlags flags = 0,
						 ImGuiInputTextCallback callback = nullptr, void *user_data = nullptr);
IMGUI_API bool InputTextMultiline(const char *, TemStream::String *, const ImVec2 &size = ImVec2(0, 0),
								  ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr,
								  void *user_data = nullptr);
} // namespace ImGui
#endif

namespace TemStream
{
#else

using String = std::string;
using String32 = std::u32string;
using StringStream = std::stringstream;
template <typename T> using List = std::vector<T>;
template <typename T> using Deque = std::deque<T>;
template <typename T> using Set = std::unordered_set<T>;
template <typename K, typename V> using Map = std::unordered_map<K, V>;

template <typename T> using LinkedList = std::list<T>;
template <typename T> using Queue = std::queue<T>;

using UTF8Converter = std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t>;

template <typename T, typename... Args> T *allocateAndConstruct(Args &&...args)
{
	return new T(std::forward<Args>(args)...);
}
template <typename T> void destroyAndDeallocate(T *const t)
{
	delete t;
}

template <typename T> using unique_ptr = std::unique_ptr<T>;

template <typename T> using shared_ptr = std::shared_ptr<T>;

template <typename T, typename... Args> static inline unique_ptr<T> tem_unique(Args &&...args)
{
	return unique_ptr<T>(allocateAndConstruct<T>(std::forward<Args>(args)...));
}

template <typename T, typename... Args> static inline shared_ptr<T> tem_shared(Args &&...args)
{
	return shared_ptr<T>(allocateAndConstruct<T>(std::forward<Args>(args)...));
}

template <typename T> static inline shared_ptr<T> tem_shared(T *t)
{
	return shared_ptr<T>(t);
}
#endif

/**
 * @brief Remove whitespace from left and right side of string
 *
 * @param string
 * @return New string
 */
extern String &trim(String &string);

/**
 * @brief Remove whitespace from left side of string
 *
 * @param string
 * @return New string
 */
extern String &ltrim(String &string);

/**
 * @brief Remove whitespace from right side of string
 *
 * @param string
 * @return New string
 */
extern String &rtrim(String &string);

using StringList = List<String>;

extern std::ostream &printMemory(std::ostream &os, const char *label, const size_t mem);

/**
 * @brief Print byte count as a human-friendly string
 *
 * @param mem The amount of bytes
 *
 * @return the string
 */
extern String printMemory(const size_t mem);

} // namespace TemStream