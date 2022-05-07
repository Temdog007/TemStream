#include <main.hpp>

bool sendAll(const int fd, const char *data, size_t size);

namespace TemStream
{
Socket::Socket() : buffer(), outgoing(KB(1)), mutex()
{
}
Socket::~Socket()
{
}
void Socket::send(const uint8_t *data, const size_t size, const bool convertToBase64)
{
	LOCK(mutex);

	if (convertToBase64)
	{
		ByteList bytes(data, size);
		bytes = base64_encode(bytes);
		outgoing.append(bytes);
		outgoing.append('\0');
	}
	else
	{
		{
			MemoryStream m;
			{
				Message::Header header;
				header.size = static_cast<uint64_t>(size);
				header.id = Message::MagicGuid;
				cereal::PortableBinaryOutputArchive ar(m);
				ar(header);
			}
			outgoing.append(m->getData(), m->getWritePoint());
		}
		outgoing.append(data, size);
	}
}
bool Socket::sendPacket(const Message::Packet &packet, const bool sendImmediately)
{
	try
	{
		MemoryStream m;
		{
			cereal::PortableBinaryOutputArchive in(m);
			in(packet);
		}
		send(m->getData(), m->getSize());
		if (sendImmediately)
		{
			return flush();
		}
		else
		{
			return true;
		}
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Socket::sendMessage " << e.what() << std::endl;
	}
	return false;
}
bool Socket::flush()
{
	ByteList t;
	{
		LOCK(mutex);
		t.swap(outgoing);
	}
	return flush(t);
}
BasicSocket::BasicSocket() : Socket(), fd(-1)
{
}
BasicSocket::BasicSocket(const int fd) : Socket(), fd(fd)
{
}
BasicSocket::~BasicSocket()
{
	close();
}
void BasicSocket::close()
{
	::close(fd);
	fd = -1;
}
PollState BasicSocket::pollRead(const int timeout) const
{
	return pollSocket(fd, timeout, POLLIN);
}
PollState BasicSocket::pollWrite(const int timeout) const
{
	return pollSocket(fd, timeout, POLLOUT);
}
bool BasicSocket::flush(const ByteList &bytes)
{
	return sendAll(fd, reinterpret_cast<const char *>(bytes.data()), bytes.size());
}
bool BasicSocket::getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &str, uint16_t &port) const
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
UdpSocket::UdpSocket() : BasicSocket()
{
}
UdpSocket::UdpSocket(const int fd) : BasicSocket(fd)
{
}
UdpSocket::~UdpSocket()
{
}
bool UdpSocket::connect(const char *hostname, const char *port, const bool isServer)
{
	close();
	return openSocket(fd, hostname, port, isServer, false);
}
void UdpSocket::send(const uint8_t *, size_t, const bool)
{
	throw std::runtime_error("Invalid call to UdpSocket::send");
}
bool UdpSocket::read(const int timeout, ByteList &bytes, const bool readAll)
{
	int reads = 0;
	struct sockaddr_storage addr;
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

		socklen_t len = sizeof(addr);
		const ssize_t r = recvfrom(fd, buffer.data(), buffer.size(), 0, (struct sockaddr *)&addr, &len);
		if (r < 0)
		{
			perror("recvfrom");
			return false;
		}
		if (r == 0)
		{
			return false;
		}
		bytes.append(buffer.begin(), r);
	} while (readAll && timeout == 0 && ++reads < 100 && bytes.size() < KB(64));
	return true;
}
TcpSocket::TcpSocket() : BasicSocket()
{
}
TcpSocket::TcpSocket(const int fd) : BasicSocket(fd)
{
}
TcpSocket::~TcpSocket()
{
}
bool TcpSocket::connect(const char *hostname, const char *port, const bool isServer)
{
	close();
	return openSocket(fd, hostname, port, isServer, true);
}
bool TcpSocket::read(const int timeout, ByteList &bytes, const bool readAll)
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

		const ssize_t r = recv(fd, buffer.data(), buffer.size(), 0);
		if (r < 0)
		{
			perror("recv");
			return false;
		}
		if (r == 0)
		{
			return false;
		}
		bytes.append(buffer.begin(), r);
	} while (readAll && timeout == 0 && ++reads < 100 && bytes.size() < KB(64));
	return true;
}
} // namespace TemStream

bool sendAll(const int fd, const char *data, size_t size)
{
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
	}
	return true;
}