#include <main.hpp>

namespace TemStream
{
const char *PeerTypeToString(const PeerType t)
{
	switch (t)
	{
	case PeerType::Consumer:
		return "Consumer";
	case PeerType::Producer:
		return "Producer";
	case PeerType::Server:
		return "Server";
	default:
		return "Invalid";
	}
}
Peer::Peer(int fd) : data(), nextMessageSize(std::nullopt), info(), fd(fd)
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

bool Peer::readData(const int timeout)
{
	switch (pollSocket(fd, timeout))
	{
	case PollState::Error:
		return false;
	case PollState::GotData:
		break;
	default:
		return true;
	}

	const ssize_t r = read(fd, buffer.data(), buffer.size());
	if (r < 0)
	{
		perror("read");
		return false;
	}
	if (r == 0)
	{
		return false;
	}

	data.insert(data.end(), buffer.begin(), buffer.begin() + r);
	if (!nextMessageSize.has_value())
	{
		if (data.size() < sizeof(uint32_t))
		{
			return true;
		}

		std::ostringstream ss;
		ss.write(data.data(), sizeof(uint32_t));
		cereal::PortableBinaryOutputArchive ar(ss);

		uint32_t value = 0;
		ar(value);
		nextMessageSize = value;

		data.erase(data.begin(), data.begin() + sizeof(uint32_t));
	}

	if (*nextMessageSize == data.size())
	{
		std::ostringstream ss;
		ss.write(data.data(), r);
		cereal::PortableBinaryOutputArchive ar(ss);

		MessagePacket packet;
		ar(packet);

		const bool result = handlePacket(packet);
		data.clear();
		nextMessageSize = std::nullopt;
		return result;
	}
	else if (*nextMessageSize < data.size())
	{
		std::ostringstream ss;
		ss.write(data.data(), r);
		cereal::PortableBinaryOutputArchive ar(ss);

		MessagePacket packet;
		ar(packet);

		const bool result = handlePacket(packet);
		data.erase(data.begin(), data.begin() + *nextMessageSize);
		nextMessageSize = std::nullopt;
		return result;
	}
	return true;
}
} // namespace TemStream