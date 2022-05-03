#include <main.hpp>

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
void Socket::sendPacket(const Message::Packet &packet)
{
	try
	{
		MemoryStream m;
		{
			cereal::PortableBinaryOutputArchive in(m);
			in(packet);
		}
		send(m->getData(), m->getSize());
	}
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Socket::sendMessage " << e.what() << std::endl;
	}
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
TcpSocket::TcpSocket() : Socket(), fd(-1)
{
}
TcpSocket::TcpSocket(const int fd) : Socket(), fd(fd)
{
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
	} while (timeout == 0 && ++reads < 100 && bytes.size() < KB(64));
	return true;
}
bool TcpSocket::flush(const ByteList &bytes)
{
	return sendAll(fd, reinterpret_cast<const char *>(bytes.data()), bytes.size());
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