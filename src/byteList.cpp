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
ByteList::ByteList() noexcept : buffer(nullptr), used(0), total(0)
{
}
ByteList::ByteList(const uint32_t initialSize) : buffer(nullptr), used(0), total(0)
{
	reallocate(initialSize);
}
ByteList::ByteList(const uint8_t *data, const uint32_t size) : buffer(nullptr), used(0), total(0)
{
	append(data, size);
}
ByteList::ByteList(const MemoryStream &m) : buffer(nullptr), used(0), total(0)
{
	*this = m->getBytes();
}
ByteList::ByteList(MemoryStream &&m) : buffer(nullptr), used(0), total(0)
{
	*this = m->moveBytes();
}
ByteList::ByteList(const ByteList &list) : buffer(nullptr), used(0), total(0)
{
	append(list);
}
ByteList::ByteList(const ByteList &list, const uint32_t len, const uint32_t offset) : buffer(nullptr), used(0), total(0)
{
	append(list, len, offset);
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
void ByteList::reallocate(const uint32_t newSize)
{
	if (total == 0 || buffer == nullptr || total < newSize)
	{
		Allocator<uint8_t> a;
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
void ByteList::append(const uint8_t *data, const uint32_t count)
{
	if (count == 0 || data == nullptr)
	{
		return;
	}
	if (used + count >= total)
	{
		reallocate(total + used + count + 1);
	}

	memcpy(&buffer[used], data, count);
	used += count;
}
void ByteList::append(const ByteList &list)
{
	append(list.buffer, list.used);
}
void ByteList::append(const ByteList &list, const uint32_t count, const uint32_t offset)
{
	if (offset + count >= list.used)
	{
		append(list.buffer + offset, list.used - offset);
	}
	else
	{
		append(list.buffer + offset, count);
	}
}
void ByteList::insert(const uint8_t *data, const uint32_t count, const uint32_t offset)
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
void ByteList::remove(const uint32_t count)
{
	if (count == 0 || empty())
	{
		return;
	}
	if (count >= used)
	{
		clear();
		return;
	}

	memmove(&buffer[0], &buffer[count], used - count);
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
