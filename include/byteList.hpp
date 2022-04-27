#pragma once

#include <main.hpp>

namespace TemStream
{
class ByteList
{
  private:
	uint8_t *buffer;
	uint32_t used;
	uint32_t total;

	void deepClear();

  public:
	ByteList() noexcept;
	ByteList(size_t initialSize);
	ByteList(const uint8_t *, size_t);
	template <typename T> ByteList(const T *t, size_t count) : buffer(nullptr), used(0), total(0)
	{
		append(t, count);
	}
	template <typename T, const size_t N> ByteList(const std::array<T, N> &arr) : buffer(nullptr), used(0), total(0)
	{
		insert(arr.data(), arr.size());
	}
	ByteList(const ByteList &);
	ByteList(ByteList &&) noexcept;
	~ByteList();

	ByteList &operator=(const ByteList &);
	ByteList &operator=(ByteList &&);

	const uint8_t &operator[](size_t) const;
	uint8_t &operator[](size_t);

	uint8_t *data()
	{
		return buffer;
	}
	const uint8_t *data() const
	{
		return buffer;
	}

	size_t size() const
	{
		return used;
	}

	bool empty() const
	{
		return used == 0 || buffer == nullptr || total == 0;
	}

	bool reallocate(size_t);

	bool append(uint8_t);

	bool append(const uint8_t *, const size_t);

	bool insert(const uint8_t *, const size_t, const size_t offset);

	bool append(const ByteList &);
	bool append(const ByteList &, uint32_t);

	template <typename Iterator> bool append(Iterator start, Iterator end)
	{
		for (auto iter = start; start != end; ++iter)
		{
			if (!append(static_cast<uint8_t>(*iter)))
			{
				return false;
			}
		}
		return true;
	}

	template <typename T> bool append(const T *t, const size_t count = 1)
	{
		return append(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	ByteList &operator+=(const ByteList &);
	ByteList operator+(const ByteList &) const;

	// Remove bytes starting from the front
	void remove(size_t);

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
		return Iterator(&buffer[0]);
	}
	auto end() const
	{
		return Iterator(&buffer[used]);
	}

	template <class Archive> void save(Archive &archive) const
	{
		archive(used);
		archive(cereal::binary_data(buffer, used));
	}

	template <class Archive> void load(Archive &archive)
	{
		archive(used);
		reallocate(used);
		archive(cereal::binary_data(buffer, used));
	}
};
} // namespace TemStream