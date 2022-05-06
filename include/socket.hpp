#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
class Packet;
}
class Socket
{
  protected:
	std::array<char, KB(64)> buffer;
	ByteList outgoing;
	Mutex mutex;

	// Ensure only one thread every calls this
	virtual bool flush(const ByteList &) = 0;

  public:
	Socket();
	virtual ~Socket();

	void sendPacket(const Message::Packet &, const bool sendImmediately = false);

	virtual bool connect(const char *hostname, const char *port, const bool isServer) = 0;
	virtual void send(const uint8_t *, size_t, const bool convertToBase64 = TEMSTREAM_USE_BASE64);
	virtual bool read(const int timeout, ByteList &, const bool readAll) = 0;

	bool flush();

	template <typename T> void send(const T *t, const size_t count)
	{
		send(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const = 0;
};
class BasicSocket : public Socket
{
  protected:
	int fd;

	bool flush(const ByteList &) override;
	void close();

  public:
	BasicSocket();
	BasicSocket(int);
	BasicSocket(const BasicSocket &) = delete;
	BasicSocket(BasicSocket &&) = delete;
	virtual ~BasicSocket();

	PollState pollRead(const int timeout) const;
	PollState pollWrite(const int timeout) const;

	bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const override;
};
class UdpSocket : public BasicSocket
{
  public:
	UdpSocket();
	UdpSocket(int);
	virtual ~UdpSocket();

	template <class S> static unique_ptr<UdpSocket> create(const BaseAddress<S> &addr)
	{
		auto ptr = tem_unique<UdpSocket>();
		char port[64];
		snprintf(port, sizeof(port), "%d", addr.port);
		if (ptr->connect(addr.hostname.c_str(), port, true))
		{
			return ptr;
		}
		return nullptr;
	}

	void send(const uint8_t *, size_t, const bool) override;

	bool connect(const char *hostname, const char *port, const bool isServer) override;
	bool read(const int timeout, ByteList &, const bool readAll) override;
};
class TcpSocket : public BasicSocket
{
  public:
	TcpSocket();
	TcpSocket(int);
	virtual ~TcpSocket();

	template <class S> static unique_ptr<TcpSocket> create(const BaseAddress<S> &addr)
	{
		auto ptr = tem_unique<TcpSocket>();
		char port[64];
		snprintf(port, sizeof(port), "%d", addr.port);
		if (ptr->connect(addr.hostname.c_str(), port, false))
		{
			return ptr;
		}
		return nullptr;
	}

	bool connect(const char *hostname, const char *port, const bool isServer) override;
	bool read(const int timeout, ByteList &, const bool readAll) override;
};
} // namespace TemStream