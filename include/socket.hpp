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

	void sendPacket(const Message::Packet &);

	virtual bool connect(const char *hostname, const char *port, const bool isServer) = 0;
	void send(const uint8_t *, size_t, const bool convertToBase64 = TEMSTREAM_USE_BASE64);
	virtual bool read(const int timeout, ByteList &) = 0;

	bool flush();

	template <typename T> void send(const T *t, const size_t count)
	{
		send(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const = 0;
};
class TcpSocket : public Socket
{
  private:
	int fd;

	bool flush(const ByteList &) override;
	void close();

  public:
	TcpSocket();
	TcpSocket(int);
	TcpSocket(const TcpSocket &) = delete;
	TcpSocket(TcpSocket &&) = delete;
	virtual ~TcpSocket();

	PollState pollRead(const int timeout) const;
	PollState pollWrite(const int timeout) const;

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

	virtual bool connect(const char *hostname, const char *port, const bool isServer) override;
	virtual bool read(const int timeout, ByteList &) override;

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const override;
};
} // namespace TemStream