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
	virtual ~MemoryBuffer();

	std::streamsize xsgetn(char *, std::streamsize) override;
	std::streamsize xsputn(const char *, std::streamsize) override;
	pos_type seekoff(off_type, std::ios_base::seekdir, std::ios_base::openmode) override;
	pos_type seekpos(pos_type, std::ios_base::openmode) override;

	const char *getData() const
	{
		return reinterpret_cast<const char *>(byteList.data());
	}
	std::size_t getSize() const
	{
		return byteList.size();
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
	virtual ~MemoryStream();

	MemoryBuffer &operator*()
	{
		return buffer;
	}
	MemoryBuffer *operator->()
	{
		return &buffer;
	}
};
} // namespace TemStream