#include <main.hpp>

namespace TemStream
{
MemoryBuffer::MemoryBuffer() : std::basic_streambuf<char>(), buffer(), writePoint(0), readPoint(0)
{
}
MemoryBuffer::~MemoryBuffer()
{
}
std::streamsize MemoryBuffer::xsgetn(char *c, const std::streamsize size)
{
	const auto left = static_cast<std::streamsize>(buffer.size() - readPoint);
	if (left == 0)
	{
		return 0;
	}

	const auto copy = left < size ? left : size;
	memcpy(c, buffer.data() + readPoint, copy);
	readPoint += copy;
	return copy;
}
std::streamsize MemoryBuffer::xsputn(const char *c, const std::streamsize size)
{
	buffer.insert(buffer.begin() + writePoint, c, c + size);
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
MemoryStream::~MemoryStream()
{
}
} // namespace TemStream