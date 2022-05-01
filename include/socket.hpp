#pragma once

#include <main.hpp>

namespace TemStream
{
namespace Message
{
class Packet;
}
class Address;
class Socket
{
  protected:
	std::array<char, KB(64)> buffer;

  public:
	Socket();
	virtual ~Socket();

	bool sendPacket(const Message::Packet &);
	bool connectWithAddress(const Address &, const bool isServer);

	virtual bool connect(const char *hostname, const char *port, const bool isServer) = 0;
	virtual bool send(const uint8_t *, size_t, const bool convertToBase64 = TEMSTREAM_USE_BASE64) = 0;
	virtual bool read(const int timeout, ByteList &) = 0;

	template <typename T> bool send(const T *t, const size_t count)
	{
		return send(reinterpret_cast<const uint8_t *>(t), sizeof(T) * count);
	}

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const = 0;
};
class TcpSocket : public Socket
{
  private:
	Mutex mutex;
	int fd;

	void close();

  public:
	TcpSocket();
	TcpSocket(int);
	TcpSocket(const TcpSocket &) = delete;
	TcpSocket(TcpSocket &&) noexcept;
	virtual ~TcpSocket();

	PollState pollRead(const int timeout) const;
	PollState pollWrite(const int timeout) const;

	virtual bool connect(const char *hostname, const char *port, const bool isServer) override;
	virtual bool send(const uint8_t *, size_t, bool) override;
	virtual bool read(const int timeout, ByteList &) override;

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const override;
};
} // namespace TemStream