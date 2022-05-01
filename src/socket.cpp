#include <main.hpp>

namespace TemStream
{
Socket::Socket() : buffer()
{
}
Socket::~Socket()
{
}
bool Socket::sendPacket(const Message::Packet &packet)
{
	try
	{
		MemoryStream m;
		{
			cereal::PortableBinaryOutputArchive in(m);
			in(packet);
		}
		return send(m.getData(), m.getSize());
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Socket::sendMessage " << e.what() << std::endl;
		return false;
	}
}
bool Socket::connectWithAddress(const Address &addr, const bool isServer)
{
	char port[64];
	snprintf(port, sizeof(port), "%d", addr.port);
	return connect(addr.hostname.c_str(), port, isServer);
}
TcpSocket::TcpSocket() : Socket(), mutex(), fd(-1)
{
}
TcpSocket::TcpSocket(const int fd) : Socket(), mutex(), fd(fd)
{
}
TcpSocket::TcpSocket(TcpSocket &&s) noexcept : Socket(s), mutex(), fd(s.fd)
{
	s.fd = -1;
}
TcpSocket::~TcpSocket()
{
	close();
}
void TcpSocket::close()
{
	::close(fd);
	fd = -1;
}
bool TcpSocket::connect(const char *hostname, const char *port, const bool isServer)
{
	close();
	return openSocket(fd, hostname, port, isServer);
}
PollState TcpSocket::pollRead(const int timeout) const
{
	return pollSocket(fd, timeout, POLLIN);
}
PollState TcpSocket::pollWrite(const int timeout) const
{
	return pollSocket(fd, timeout, POLLOUT);
}
bool TcpSocket::send(const uint8_t *data, size_t size)
{
	switch (pollWrite(100))
	{
	case PollState::GotData:
		break;
	default:
		return false;
	}
	LOCK(mutex);
	{
		uint32_t u = static_cast<uint32_t>(size);
		u = htonl(u);
		const uint8_t *uSize = reinterpret_cast<const uint8_t *>(&u);
		size_t written = 0;
		while (written < sizeof(uint32_t))
		{
			const ssize_t sent = ::send(fd, uSize + written, sizeof(uint32_t) - written, 0);
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
			if (written != sizeof(uint32_t))
			{
				printf("Wrote %zu out of 4 bytes\n", written);
			}
		}
	}
	size_t written = 0;
	while (written < size)
	{
		const ssize_t sent = ::send(fd, data + written, size - written, 0);
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
		if (written != size)
		{
			printf("Wrote %zu out of %zu bytes\n", written, size);
		}
	}
	return true;
}
bool TcpSocket::read(const int timeout, ByteList &bytes)
{
	int reads = 0;
	do
	{
		switch (pollRead(timeout))
		{
		case PollState::Error:
			return false;
		case PollState::GotData:
			break;
		default:
			return true;
		}

		LOCK(mutex);
		const ssize_t r = ::read(fd, buffer.data(), buffer.size());
		if (r < 0)
		{
			perror("read");
			return false;
		}
		if (r == 0)
		{
			return false;
		}
		bytes.append(buffer.begin(), r);
	} while (timeout == 0 && ++reads < 100 && bytes.size() < KB(64));
	return true;
}
bool TcpSocket::getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &str, uint16_t &port) const
{
	struct sockaddr_storage addr;
	socklen_t size = sizeof(addr);
	if (getpeername(fd, (struct sockaddr *)&addr, &size) < 0)
	{
		perror("getpeername");
		return false;
	}

	if (addr.ss_family == AF_INET)
	{
		struct sockaddr_in *s = (struct sockaddr_in *)&addr;
		inet_ntop(addr.ss_family, &s->sin_addr, str.data(), str.size());
		port = ntohs(s->sin_port);
	}
	else
	{
		struct sockaddr_in6 *s = (struct sockaddr_in6 *)&addr;
		inet_ntop(addr.ss_family, &s->sin6_addr, str.data(), str.size());
		port = ntohs(s->sin6_port);
	}
	return true;
}
} // namespace TemStream