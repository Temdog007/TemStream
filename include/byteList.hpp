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
class MemoryStream;
class ByteList
{
  private:
	uint8_t *buffer;
	uint32_t used;
	uint32_t total;

	void deepClear();

  public:
	ByteList() noexcept;
	ByteList(uint32_t initialSize);
	ByteList(const uint8_t *, uint32_t);
	ByteList(const MemoryStream &);
	ByteList(MemoryStream &&);
	template <typename T> ByteList(const T *t, const uint32_t count) : buffer(nullptr), used(0), total(0)
	{
		append(t, count);
	}
	template <typename T, const size_t N> ByteList(const std::array<T, N> &arr) : buffer(nullptr), used(0), total(0)
	{
		insert(arr.data(), arr.size());
	}
	ByteList(const ByteList &);
	ByteList(const ByteList &, uint32_t len, const uint32_t offset = 0);
	ByteList(ByteList &&) noexcept;
	~ByteList();

	ByteList &operator=(const ByteList &);
	ByteList &operator=(ByteList &&);

	const uint8_t &operator[](size_t) const;
	uint8_t &operator[](size_t);

	constexpr uint8_t *data()
	{
		return buffer;
	}
	constexpr const uint8_t *data() const
	{
		return buffer;
	}

	template <typename T> constexpr const T *data() const
	{
		return reinterpret_cast<const T *>(buffer);
	}

	constexpr uint32_t size() const
	{
		return used;
	}

	template <typename T> constexpr T size() const
	{
		return static_cast<T>(used);
	}

	constexpr bool empty() const
	{
		return used == 0 || buffer == nullptr || total == 0;
	}

	void reallocate(uint32_t);

	void append(uint8_t);

	void append(const uint8_t *, const uint32_t);

	void insert(const uint8_t *, const uint32_t, const uint32_t offset);

	void append(const ByteList &);
	void append(const ByteList &, const uint32_t len, const uint32_t offset = 0);

	template <typename Iterator> void append(Iterator start, Iterator end)
	{
		for (auto iter = start; iter != end; ++iter)
		{
			append(static_cast<uint8_t>(*iter));
		}
	}

	template <typename T> void append(const T *t, const uint32_t count = 1)
	{
		return append(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	ByteList &operator+=(const ByteList &);
	ByteList operator+(const ByteList &) const;

	// Remove bytes starting from the front
	void remove(uint32_t);

	void clear(bool deep = false);

	void swap(ByteList &) noexcept;

	struct Iterator
	{
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint8_t;
		using pointer = uint8_t *;
		using reference = uint8_t &;

		Iterator(pointer ptr) : m_ptr(ptr)
		{
		}

		reference operator*() const
		{
			return *m_ptr;
		}
		pointer operator->()
		{
			return m_ptr;
		}
		Iterator &operator++()
		{
			m_ptr++;
			return *this;
		}
		Iterator operator++(int)
		{
			Iterator tmp = *this;
			++(*this);
			return tmp;
		}
		friend bool operator==(const Iterator &a, const Iterator &b)
		{
			return a.m_ptr == b.m_ptr;
		};
		friend bool operator!=(const Iterator &a, const Iterator &b)
		{
			return a.m_ptr != b.m_ptr;
		};

	  private:
		pointer m_ptr;
	};

	struct ConstIterator
	{
		using iterator_category = std::forward_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = uint8_t;
		using pointer = const uint8_t *;
		using reference = const uint8_t &;

		ConstIterator(pointer ptr) : m_ptr(ptr)
		{
		}

		reference operator*() const
		{
			return *m_ptr;
		}
		pointer operator->()
		{
			return m_ptr;
		}
		ConstIterator &operator++()
		{
			m_ptr++;
			return *this;
		}
		ConstIterator operator++(int)
		{
			ConstIterator tmp = *this;
			++(*this);
			return tmp;
		}
		friend bool operator==(const ConstIterator &a, const ConstIterator &b)
		{
			return a.m_ptr == b.m_ptr;
		};
		friend bool operator!=(const ConstIterator &a, const ConstIterator &b)
		{
			return a.m_ptr != b.m_ptr;
		};

	  private:
		pointer m_ptr;
	};

	auto begin()
	{
		return Iterator(&buffer[0]);
	}
	auto end()
	{
		return Iterator(&buffer[used]);
	}

	auto begin() const
	{
		return ConstIterator(&buffer[0]);
	}
	auto end() const
	{
		return ConstIterator(&buffer[used]);
	}

	auto cbegin() const
	{
		return ConstIterator(&buffer[0]);
	}
	auto cend() const
	{
		return ConstIterator(&buffer[used]);
	}

	/**
	 * Save for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void save(Archive &archive) const
	{
		archive(cereal::make_size_tag(used));
		archive(cereal::binary_data(buffer, used));
	}

	/**
	 * Loading for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void load(Archive &archive)
	{
		archive(cereal::make_size_tag(used));
		reallocate(used);
		archive(cereal::binary_data(buffer, used));
	}
};
} // namespace TemStream