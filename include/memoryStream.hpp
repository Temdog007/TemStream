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