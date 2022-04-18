#include <main.hpp>

namespace TemStream
{
Socket::Socket() : buffer()
{
}
Socket::~Socket()
{
}
bool Socket::sendPacket(const MessagePacket &packet)
{
	try
	{
		MemoryStream m;
		cereal::PortableBinaryOutputArchive in(m);
		in(packet);
		return send(m.getData(), m.getSize());
	}
	catch (const std::exception &e)
	{
		logger->AddError("Socket::sendMessage %s", e.what());
		return false;
	}
}
bool Socket::connectWithAddress(const Address &addr, const bool isServer)
{
	char port[64];
	snprintf(port, sizeof(port), "%d", addr.port);
	return connect(addr.hostname.c_str(), port, isServer);
}
TcpSocket::TcpSocket() : Socket(), fd(-1)
{
}
TcpSocket::TcpSocket(const int fd) : Socket(), fd(fd)
{
}
TcpSocket::TcpSocket(TcpSocket &&s) noexcept : Socket(s), fd(s.fd)
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
bool TcpSocket::send(const void *data, size_t size)
{
	{
		uint32_t u = static_cast<uint32_t>(size);
		u = htonl(u);
		if (::send(fd, &u, sizeof(u), 0) != sizeof(u))
		{
			perror("send");
			return false;
		}
	}
	size_t written = 0;
	while (written < size)
	{
		const ssize_t sent = ::send(fd, data, size, 0);
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
bool TcpSocket::read(const int timeout, Bytes &bytes)
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
	bytes.insert(bytes.end(), buffer.begin(), buffer.begin() + r);
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