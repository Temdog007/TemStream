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
MemoryBuffer::MemoryBuffer() : std::basic_streambuf<char>(), byteList(), writePoint(0), readPoint(0)
{
}
MemoryBuffer::MemoryBuffer(const ByteList &bytes)
	: std::basic_streambuf<char>(), byteList(bytes), writePoint(byteList.size()), readPoint(0)
{
}
MemoryBuffer::MemoryBuffer(ByteList &&bytes)
	: std::basic_streambuf<char>(), byteList(std::move(bytes)), writePoint(byteList.size()), readPoint(0)
{
}
MemoryBuffer::~MemoryBuffer()
{
}
std::streamsize MemoryBuffer::xsgetn(char *c, const std::streamsize size)
{
	const auto left = static_cast<std::streamsize>(byteList.size() - readPoint);
	if (left == 0)
	{
		return 0;
	}

	const auto toCopy = std::min<std::streamsize>(left, size);
	memcpy(c, byteList.data() + readPoint, toCopy);
	readPoint += toCopy;
	return toCopy;
}
std::streamsize MemoryBuffer::xsputn(const char *c, const std::streamsize size)
{
	byteList.insert(reinterpret_cast<const uint8_t *>(c), static_cast<uint32_t>(size),
					static_cast<uint32_t>(writePoint));
	writePoint += size;
	return size;
}
MemoryBuffer::pos_type MemoryBuffer::seekoff(const off_type offset, const std::ios_base::seekdir dir,
											 const std::ios_base::openmode mode)
{
	std::streampos p(-1);
	if (dir > 0)
	{
		if ((mode & std::ios_base::out) != 0)
		{
			writePoint += offset;
			p = std::streampos(writePoint);
		}
		if ((mode & std::ios_base::in) != 0)
		{
			readPoint += offset;
			p = std::streampos(readPoint);
		}
	}
	else
	{
		if ((mode & std::ios_base::out) != 0)
		{
			writePoint -= offset;
			p = std::streampos(writePoint);
		}
		if ((mode & std::ios_base::in) != 0)
		{
			readPoint -= offset;
			p = std::streampos(readPoint);
		}
	}
	return p;
}
MemoryBuffer::pos_type MemoryBuffer::seekpos(const pos_type pos, const std::ios_base::openmode mode)
{
	std::streamsize p(-1);
	if ((mode & std::ios_base::out) != 0)
	{
		writePoint = pos;
		p = writePoint;
	}
	if ((mode & std::ios_base::in) != 0)
	{
		readPoint = pos;
		p = readPoint;
	}
	return p;
}
MemoryStream::MemoryStream() : std::iostream(&buffer), buffer()
{
}
MemoryStream::MemoryStream(const ByteList &bytes) : std::iostream(&buffer), buffer(bytes)
{
}
MemoryStream::MemoryStream(ByteList &&bytes) : std::iostream(&buffer), buffer(std::move(bytes))
{
}
MemoryStream::~MemoryStream()
{
}
} // namespace TemStream