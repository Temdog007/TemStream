#include <main.hpp>

namespace TemStream
{
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

bool Peer::sendMessage(const MessagePacket &packet) const
{
	try
	{
		MemoryStream m;
		cereal::PortableBinaryOutputArchive in(m);
		in(packet);
		return sendData(m.getData(), m.getSize());
	}
	catch (const std::exception &e)
	{
		fprintf(stderr, "Peer::sendMessage %s\n", e.what());
		return false;
	}
}

bool Peer::sendData(const void *data, const size_t size) const
{
	{
		uint32_t u = static_cast<uint32_t>(size);
		u = htonl(u);
		if (send(fd, &u, sizeof(u), 0) != sizeof(u))
		{
			perror("send");
			return false;
		}
	}
	size_t written = 0;
	while (written < size)
	{
		const ssize_t sent = send(fd, data, size, 0);
		if (sent < 0)
		{
			perror("send");
			return false;
		}
		if (sent == 0)
		{
			return false;
		}
		written += sent;
	}
	return true;
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
	try
	{
		if (!nextMessageSize.has_value())
		{
			if (data.size() < sizeof(uint32_t))
			{
				return true;
			}

			union {
				uint32_t value;
				char data[4];
			} v;

			for (size_t i = 0; i < 4; ++i)
			{
				v.data[i] = data[i];
			}
			nextMessageSize = ntohl(v.value);

			data.erase(data.begin(), data.begin() + sizeof(uint32_t));
		}

		if (*nextMessageSize == data.size())
		{
			MemoryStream m;
			m.write(data.data(), data.size());
			cereal::PortableBinaryInputArchive ar(m);

			MessagePacket packet;
			ar(packet);

			const bool result = handlePacket(packet);
			data.clear();
			nextMessageSize = std::nullopt;
			return result;
		}
		else if (*nextMessageSize < data.size())
		{
			MemoryStream m;
			m.write(data.data(), *nextMessageSize);
			cereal::PortableBinaryInputArchive ar(m);

			MessagePacket packet;
			ar(packet);

			const bool result = handlePacket(packet);
			data.erase(data.begin(), data.begin() + *nextMessageSize);
			nextMessageSize = std::nullopt;
			return result;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		fprintf(stderr, "Peer::readData: %s\n", e.what());
		return false;
	}
}
} // namespace TemStream