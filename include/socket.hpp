#pragma once

#include <main.hpp>

namespace TemStream
{
class ByteList;
namespace Message
{
struct Packet;
}
class Socket
{
  protected:
	std::array<char, KB(64)> buffer;
	ByteList outgoing;
	Mutex mutex;

	/**
	 * Send all bytes in the outgoing list to the peer. Ensure only one thread every calls this
	 *
	 * @param bytes
	 *
	 * @return True if successful
	 */
	virtual bool flush(const ByteList &) = 0;

  public:
	Socket();
	virtual ~Socket();

	bool sendPacket(const Message::Packet &, const bool sendImmediately = false);

	virtual bool connect(const char *hostname, const char *port) = 0;
	virtual void send(const uint8_t *, uint32_t);
	virtual bool read(const int timeout, ByteList &, const bool readAll) = 0;

	/**
	 * Copies outgoing to temporary and calls ::flush(const ByteList&) with the temporary byte list. This is to avoid
	 * locking the outgoing list to prevent receiving data from peer in another thread.
	 *
	 * @param bytes
	 *
	 * @return True if successful
	 */
	bool flush();

	template <typename T> void send(const T *t, const uint32_t count)
	{
		send(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	void send(const ByteList &);

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const = 0;
};
class BasicSocket : public Socket
{
  protected:
	SOCKET fd;

	virtual bool flush(const ByteList &) override;
	void close();

	BasicSocket(BasicSocket &&);

  public:
	BasicSocket();
	BasicSocket(SOCKET);
	BasicSocket(const BasicSocket &) = delete;

	virtual ~BasicSocket();

	PollState pollRead(const int timeout) const;
	PollState pollWrite(const int timeout) const;

	bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const override;
};
class UdpSocket : public BasicSocket
{
  public:
	UdpSocket();
	UdpSocket(SOCKET);
	virtual ~UdpSocket();

	void send(const uint8_t *, uint32_t) override;

	bool connect(const char *hostname, const char *port) override;
	bool read(const int timeout, ByteList &, const bool readAll) override;
};
class TcpSocket : public BasicSocket
{
	friend class SSLSocket;

  protected:
	TcpSocket(TcpSocket &&);

  public:
	TcpSocket();
	TcpSocket(SOCKET);
	virtual ~TcpSocket();

	virtual bool connect(const char *hostname, const char *port) override;
	virtual bool read(const int timeout, ByteList &, const bool readAll) override;

	virtual unique_ptr<TcpSocket> acceptConnection(bool &, const int timeout = 1000) const;
};
struct SSL_Deleter
{
	void operator()(SSL *ssl) const
	{
		if (ssl != nullptr)
		{
			SSL_shutdown(ssl);
			SSL_free(ssl);
		}
	}
};
using SSLptr = std::unique_ptr<SSL, SSL_Deleter>;
struct SSL_CTX_Deleter
{
	void operator()(SSL_CTX *ctx) const
	{
		if (ctx != nullptr)
		{
			SSL_CTX_free(ctx);
		}
	}
};
using SSLContext = std::unique_ptr<SSL_CTX, SSL_CTX_Deleter>;
class SSLSocket : public TcpSocket
{
  private:
	std::variant<SSLContext, SSLptr, std::pair<SSLContext, SSLptr>> data;

	static SSLContext createContext();

	bool flush(const ByteList &) override;

  public:
	SSLSocket();
	SSLSocket(SOCKET);
	SSLSocket(SSLSocket &&) = delete;
	SSLSocket(TcpSocket &&, SSLptr &&);
	~SSLSocket();

	static const char *cert;
	static const char *key;

	bool connect(const char *hostname, const char *port) override;
	bool read(const int timeout, ByteList &, const bool readAll) override;

	unique_ptr<TcpSocket> acceptConnection(bool &, const int timeout = 1000) const override;
};
} // namespace TemStream