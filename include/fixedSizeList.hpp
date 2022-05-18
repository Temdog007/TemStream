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
template <class T, const size_t N> class FixedSizeList
{
  private:
	std::array<T, N> buffer;
	size_t startPoint;
	size_t endPoint;

	void reset()
	{
		const auto s = size();
		memmove(buffer.data(), data(), s);
		startPoint = 0;
		endPoint = s;
	}

	template <class T2, const size_t N2> friend class FixedSizeList;

  public:
	FixedSizeList() : buffer(), startPoint(0), endPoint(0)
	{
	}
	~FixedSizeList()
	{
	}

	template <class T2, const size_t N2> FixedSizeList &operator=(const FixedSizeList<T2, N2> &list)
	{
		clear();
		append(list);
		return *this;
	}

	template <class T2, const size_t N2> FixedSizeList &operator=(FixedSizeList<T2, N2> &&list)
	{
		clear();
		append(list);
		list.clear();
		return *this;
	}

	constexpr size_t size() const
	{
		return endPoint - startPoint;
	}

	constexpr bool empty() const
	{
		return size() == 0;
	}

	void clear() noexcept
	{
		startPoint = endPoint = 0;
	}

	constexpr auto begin() noexcept
	{
		return buffer.begin() + startPoint;
	}

	constexpr auto end() noexcept
	{
		return buffer.begin() + endPoint;
	}

	constexpr auto begin() const noexcept
	{
		return buffer.begin() + startPoint;
	}

	constexpr auto end() const noexcept
	{
		return buffer.begin() + endPoint;
	}

	constexpr const uint8_t *data() const noexcept
	{
		return &*begin();
	}

	constexpr uint8_t *data() noexcept
	{
		return &*begin();
	}

	bool append(const T *data, const size_t count)
	{
		if (size() + count > N)
		{
			return false;
		}
		if (endPoint + count > N)
		{
			reset();
		}
		memcpy(&*end(), data, count * sizeof(T));
		endPoint += count;
		return true;
	}

	template <class T2, const size_t N2> bool append(const FixedSizeList<T2, N2> &list)
	{
		return append(list.data(), list.size());
	}

	template <class U> bool append(const U *data, const size_t count)
	{
		return append(reinterpret_cast<const T *>(data), (count * sizeof(U)) / sizeof(T));
	}

	bool prepend(const T *data, const size_t count)
	{
		if (empty())
		{
			return append(data, count);
		}
		if (size() + count > N)
		{
			return false;
		}
		if (endPoint + count > N)
		{
			reset();
		}
		memmove(this->data() + count, this->data(), size() * sizeof(T));
		memcpy(this->data(), data, count * sizeof(T));
		endPoint += count;
		return true;
	}

	template <class T2, const size_t N2> bool prepend(const FixedSizeList<T2, N2> &list)
	{
		return prepend(list.data(), list.size());
	}

	template <class U> bool prepend(const U *data, const size_t count)
	{
		return prepend(reinterpret_cast<const T *>(data), (count * sizeof(U)) / sizeof(T));
	}

	size_t peek(T *dst, const size_t count) const
	{
		const auto toCopy = std::min(size(), count);
		memcpy(dst, data(), toCopy * sizeof(T));
		return toCopy;
	}

	size_t peek(ByteList &list, const std::optional<size_t> &max = std::nullopt) const
	{
		const auto s = max.value_or(size());
		list.append(data(), static_cast<uint32_t>(s * sizeof(T)));
		return s;
	}

	ByteList peek(const std::optional<size_t> &max = std::nullopt) const
	{
		const auto s = max.value_or(size());
		ByteList list(s);
		peek(list);
		return list;
	}

	size_t remove(size_t count)
	{
		count = std::min(count, size());
		startPoint += count;
		if (startPoint >= endPoint)
		{
			startPoint = endPoint = 0;
		}
		return count;
	}

	size_t pop(T *dst, const size_t count)
	{
		const auto copied = peek(dst, count);
		return remove(copied);
	}

	template <typename U> size_t pop(U *dst, const size_t count)
	{
		return pop(reinterpret_cast<T *>(dst), (count * sizeof(U)) / sizeof(T));
	}

	size_t pop(ByteList &list, const std::optional<size_t> &max = std::nullopt)
	{
		const auto copied = peek(list, max);
		return remove(copied);
	}

	ByteList pop(const std::optional<size_t> &max = std::nullopt)
	{
		ByteList list = peek(max);
		remove(list.size());
		return list;
	}
};
} // namespace TemStream