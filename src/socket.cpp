#include <main.hpp>

bool sendAll(const int fd, const void *, size_t size);
bool writeAll(SSL *, const void *, int);
int LogError(const char *str, size_t len, void *u);

namespace TemStream
{
Socket::Socket() : buffer(), outgoing(KB(1)), mutex()
{
}
Socket::~Socket()
{
}
void Socket::send(const ByteList &bytes)
{
	send(bytes.data(), bytes.size());
}
void Socket::send(const uint8_t *data, const size_t size)
{
	LOCK(mutex);
	{
		MemoryStream m;
		{
			Message::Header header;
			header.size = static_cast<uint64_t>(size);
			header.id = Message::MagicGuid;
			cereal::PortableBinaryOutputArchive ar(m);
			ar(header);
		}
		outgoing.append(m->getBytes());
	}
	outgoing.append(data, size);
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
		send(m->getBytes());
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
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Socket::sendMessage " << e.what() << std::endl;
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
BasicSocket::BasicSocket(BasicSocket &&b) : Socket(), fd(b.fd)
{
	b.fd = -1;
}
BasicSocket::~BasicSocket()
{
	close();
}
void BasicSocket::close()
{
	if (fd > 0)
	{
		(*logger)(Logger::Level::Trace) << "Closed socket: " << fd << std::endl;
		::close(fd);
	}
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
	return sendAll(fd, bytes.data(), bytes.size());
}
bool BasicSocket::getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &str, uint16_t &port) const
{
	struct sockaddr_storage addr;
	socklen_t size = sizeof(addr);
	if (getpeername(fd, (struct sockaddr *)&addr, &size) < 0)
	{
		// (*logger)(Logger::Level::Error) << "Error with socket: " << fd << std::endl;
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
bool UdpSocket::connect(const char *hostname, const char *port)
{
	close();
	return openSocket(fd, hostname, port, SocketType::Server, false);
}
void UdpSocket::send(const uint8_t *, size_t)
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
TcpSocket::TcpSocket(TcpSocket &&s) : BasicSocket(std::move(s))
{
}
TcpSocket::~TcpSocket()
{
}
bool TcpSocket::connect(const char *hostname, const char *port)
{
	close();
	return openSocket(fd, hostname, port, SocketType::Client, true);
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
unique_ptr<TcpSocket> TcpSocket::acceptConnection(bool &error, const int timeout) const
{
	switch (pollSocket(fd, timeout, POLLIN))
	{
	case PollState::GotData:
		break;
	case PollState::Error:
		error = true;
		return nullptr;
	default:
		return nullptr;
	}

	struct sockaddr_storage addr;
	socklen_t size = sizeof(addr);
	const int newfd = accept(fd, (struct sockaddr *)&addr, &size);
	if (newfd < 0)
	{
		perror("accept");
		return nullptr;
	}

	return tem_unique<TcpSocket>(newfd);
}
String SSLSocket::cert;
String SSLSocket::key;
SSLSocket::SSLSocket() : TcpSocket(), data(SSLptr(nullptr))
{
}
SSLSocket::SSLSocket(const int fd) : TcpSocket(fd), data(createContext())
{
}
SSLSocket::SSLSocket(TcpSocket &&tcp, SSLptr &&s) : TcpSocket(std::move(tcp)), data(std::move(s))
{
}
SSLSocket::~SSLSocket()
{
}
SSLContext SSLSocket::createContext()
{
	const SSL_METHOD *method = TLS_server_method();

	SSL_CTX *ctx = SSL_CTX_new(method);
	if (!ctx)
	{
		perror("Unable to create SSL context");
		ERR_print_errors_cb(LogError, nullptr);
		throw std::runtime_error("Unable to create SSL context");
	}

	if (SSL_CTX_use_certificate_file(ctx, SSLSocket::cert.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_cb(LogError, nullptr);
		throw std::runtime_error("Unable to use certificate file");
	}

	if (SSL_CTX_use_PrivateKey_file(ctx, SSLSocket::key.c_str(), SSL_FILETYPE_PEM) <= 0)
	{
		ERR_print_errors_cb(LogError, nullptr);
		throw std::runtime_error("Unable to use private key");
	}

	return SSLContext(ctx);
}
bool SSLSocket::flush(const ByteList &bytes)
{
	struct Foo
	{
		const ByteList &bytes;

		bool operator()(SSLptr &ptr)
		{
			return writeAll(ptr.get(), bytes.data(), bytes.size());
		}
		bool operator()(SSLContext &)
		{
			return false;
		}
		bool operator()(std::pair<SSLContext, SSLptr> &pair)
		{
			return operator()(pair.second);
		}
	};
	return std::visit(Foo{bytes}, data);
}
bool SSLSocket::connect(const char *hostname, const char *port)
{
	close();
	if (!openSocket(fd, hostname, port, SocketType::Client, true))
	{
		return false;
	}

	const SSL_METHOD *method = TLS_client_method();
	auto ctx = SSLContext(SSL_CTX_new(method));
	auto ssl = SSLptr(SSL_new(ctx.get()));
	if (ssl == nullptr)
	{
		ERR_print_errors_cb(LogError, nullptr);
		return false;
	}

	SSL_set_fd(ssl.get(), fd);
	int err = SSL_connect(ssl.get());
	if (err <= 0)
	{
		perror("SSL_connect");
		ERR_print_errors_cb(LogError, nullptr);
		return false;
	}

	auto pair = std::make_pair(std::move(ctx), std::move(ssl));
	data.emplace<std::pair<SSLContext, SSLptr>>(std::move(pair));

	return true;
}
bool SSLSocket::read(const int timeout, ByteList &bytes, const bool readAll)
{
	struct Foo
	{
		SSLSocket &s;
		ByteList &bytes;
		int timeout;
		bool readAll;

		bool operator()(SSLptr &ptr)
		{
			int reads = 0;
			do
			{
				switch (s.pollRead(timeout))
				{
				case PollState::Error:
					return false;
				case PollState::GotData:
					break;
				default:
					return true;
				}

				const ssize_t r = SSL_read(ptr.get(), s.buffer.data(), s.buffer.size());
				if (r < 0)
				{
					ERR_print_errors_cb(LogError, nullptr);
					perror("SSL_read");
					return false;
				}
				if (r == 0)
				{
					return false;
				}
				bytes.append(s.buffer.begin(), r);
			} while (readAll && timeout == 0 && ++reads < 100 && bytes.size() < KB(64));
			return true;
		}
		bool operator()(SSLContext &)
		{
			return false;
		}
		bool operator()(std::pair<SSLContext, SSLptr> &pair)
		{
			return operator()(pair.second);
		}
	};
	return std::visit(Foo{*this, bytes, timeout, readAll}, data);
}
unique_ptr<TcpSocket> SSLSocket::acceptConnection(bool &error, const int timeout) const
{
	auto ptr = TcpSocket::acceptConnection(error, timeout);
	if (ptr == nullptr)
	{
		return nullptr;
	}

	auto &ctx = std::get<SSLContext>(data);
	auto ssl = SSLptr(SSL_new(ctx.get()));
	SSL_set_fd(ssl.get(), ptr->fd);

	if (SSL_accept(ssl.get()) <= 0)
	{
		ERR_print_errors_cb(LogError, nullptr);
		return nullptr;
	}

	return tem_unique<SSLSocket>(std::move(*ptr), std::move(ssl));
}
} // namespace TemStream

bool sendAll(const int fd, const void *ptr, size_t size)
{
	size_t written = 0;
	const char *data = reinterpret_cast<const char *>(ptr);
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

bool writeAll(SSL *ssl, const void *ptr, const int size)
{
	int written = 0;
	const char *data = reinterpret_cast<const char *>(ptr);
	while (written < size)
	{
		const int sent = SSL_write(ssl, data + written, size - written);
		if (sent < 0)
		{
			perror("SSL_write");
			ERR_print_errors_cb(LogError, nullptr);
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

int LogError(const char *str, size_t len, void *)
{
	using namespace TemStream;
	String s(str, len);
	(*logger)(Logger::Level::Error) << s << std::endl;
	return 0;
}