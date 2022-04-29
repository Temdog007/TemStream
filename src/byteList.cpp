#include <main.hpp>

namespace TemStream
{
ByteList::ByteList() noexcept : buffer(nullptr), used(0), total(0)
{
}
ByteList::ByteList(const size_t initialSize) : buffer(nullptr), used(0), total(0)
{
	reallocate(initialSize);
}
ByteList::ByteList(const uint8_t *data, const size_t size) : buffer(nullptr), used(0), total(0)
{
	append(data, size);
}
ByteList::ByteList(const ByteList &list) : buffer(nullptr), used(0), total(0)
{
	append(list);
}
ByteList::ByteList(ByteList &&list) noexcept : buffer(nullptr), used(0), total(0)
{
	swap(list);
}
ByteList::~ByteList()
{
	deepClear();
}
void ByteList::deepClear()
{
	Allocator<uint8_t> a;
	a.deallocate(buffer);
	buffer = nullptr;
	used = 0;
	total = 0;
}
ByteList &ByteList::operator=(const ByteList &list)
{
	clear();
	append(list);
	return *this;
}
ByteList &ByteList::operator=(ByteList &&list)
{
	swap(list);
	list.clear(true);
	return *this;
}
const uint8_t &ByteList::operator[](const size_t index) const
{
	return buffer[index];
}
uint8_t &ByteList::operator[](const size_t index)
{
	return buffer[index];
}
void ByteList::reallocate(const size_t newSize)
{
	Allocator<uint8_t> a;
	if (total == 0 || buffer == nullptr || total < newSize)
	{
		buffer = a.reallocate(buffer, newSize);
		total = newSize;
	}
}
void ByteList::append(uint8_t d)
{
	if (total == 0 || buffer == nullptr)
	{
		reallocate(KB(1));
	}
	else if (used == total)
	{
		reallocate(total * 2);
	}

	buffer[used] = d;
	++used;
}
void ByteList::append(const uint8_t *data, const size_t count)
{
	if (count == 0 || data == nullptr)
	{
		return;
	}
	if (used + count >= total)
	{
		reallocate(total + used + count);
	}

	memcpy(&buffer[used], data, count);
	used += count;
}
void ByteList::append(const ByteList &list)
{
	append(list.buffer, list.used);
}
void ByteList::append(const ByteList &list, const uint32_t count)
{
	append(list.buffer, std::min(list.used, count));
}
void ByteList::insert(const uint8_t *data, const size_t count, const size_t offset)
{
	if (offset >= used)
	{
		append(data, count);
		return;
	}
	ByteList first(this->buffer, offset);
	ByteList middle(data, count);
	ByteList last(this->buffer + offset, used - offset);
	*this = first + middle + last;
}
ByteList ByteList::operator+(const ByteList &other) const
{
	ByteList n(*this);
	n += other;
	return n;
}
ByteList &ByteList::operator+=(const ByteList &other)
{
	append(other);
	return *this;
}
void ByteList::remove(const size_t count)
{
	if (count >= used)
	{
		clear();
		return;
	}

	memcpy(&buffer[0], &buffer[count], used - count);
	used -= count;
}
void ByteList::clear(bool deep)
{
	if (deep)
	{
		deepClear();
	}
	else
	{
		used = 0;
	}
}
void ByteList::swap(ByteList &list) noexcept
{
	std::swap(buffer, list.buffer);
	std::swap(used, list.used);
	std::swap(total, list.total);
}
} // namespace TemStream
