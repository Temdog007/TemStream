#include <main.hpp>

namespace TemStream
{
Peer::Peer() : data(), info(), nextMessageSize(std::nullopt), fd(-1)
{
}

Peer::~Peer()
{
	close();
}

void Peer::close()
{
	::close(fd);
	fd = -1;
}

bool Peer::readData()
{
	std::array<uint8_t, KB(8)> buffer;
	ssize_t r = read(fd, buffer.data(), sizeof(buffer));
	if (r < 0)
	{
		perror("read");
		return false;
	}
	if (r == 0)
	{
		return false;
	}

	data.insert(data.end(), buffer.begin(), buffer.end());
	if (!nextMessageSize.has_value())
	{
		if (data.size() < sizeof(uint32_t))
		{
			return true;
		}
		std::stringstream ss;
		for (int i = 0; i < sizeof(uint32_t); ++i)
		{
			ss << data.at(i);
		}
		cereal::PortableBinaryOutputArchive ar(ss);
		uint32_t value = 0;
		ar(value);
		nextMessageSize = value;

		data.erase(data.begin(), data.begin() + sizeof(uint32_t));
	}

	if (*nextMessageSize == data.size())
	{
		const bool result = handleData(data);
		data.clear();
		nextMessageSize = std::nullopt;
		return result;
	}
	else if (*nextMessageSize < data.size())
	{
		std::vector newV(data.begin() + *nextMessageSize, data.end());
		data.erase(data.begin() + *nextMessageSize, data.end());
		const bool result = handleData(data);
		nextMessageSize = std::nullopt;
		data = std::move(newV);
		return result;
	}
	return true;
}
} // namespace TemStream