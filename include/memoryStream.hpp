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
class MemoryBuffer : public std::basic_streambuf<char>
{
  private:
	ByteList byteList;
	std::streamsize writePoint, readPoint;

  public:
	MemoryBuffer();
	MemoryBuffer(const ByteList &);
	MemoryBuffer(ByteList &&);
	virtual ~MemoryBuffer();

	std::streamsize xsgetn(char *, std::streamsize) override;
	std::streamsize xsputn(const char *, std::streamsize) override;
	pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override;
	pos_type seekpos(pos_type, std::ios_base::openmode) override;

	ByteList &&moveBytes()
	{
		writePoint = 0;
		readPoint = 0;
		return std::move(byteList);
	}

	const ByteList &getBytes() const
	{
		return byteList;
	}

	std::streamsize getReadPoint() const
	{
		return readPoint;
	}
	std::streamsize getWritePoint() const
	{
		return writePoint;
	}
};
class MemoryStream : public std::iostream
{
  private:
	MemoryBuffer buffer;

  public:
	MemoryStream();
	MemoryStream(const ByteList &);
	MemoryStream(ByteList &&);
	virtual ~MemoryStream();

	MemoryBuffer &operator*()
	{
		return buffer;
	}
	const MemoryBuffer &operator*() const
	{
		return buffer;
	}
	MemoryBuffer *operator->()
	{
		return &buffer;
	}
	const MemoryBuffer *operator->() const
	{
		return &buffer;
	}
};
} // namespace TemStream