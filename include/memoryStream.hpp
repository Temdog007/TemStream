#pragma once

#include <main.hpp>

namespace TemStream
{
class MemoryBuffer : public std::basic_streambuf<char>
{
  private:
	std::vector<char> buffer;
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
		return buffer.data();
	}
	std::size_t getSize() const
	{
		return buffer.size();
	}
};
class MemoryStream : public std::iostream
{
  private:
	MemoryBuffer buffer;

  public:
	MemoryStream();
	virtual ~MemoryStream();

	const char *getData() const
	{
		return buffer.getData();
	}
	std::size_t getSize() const
	{
		return buffer.getSize();
	}
};
} // namespace TemStream