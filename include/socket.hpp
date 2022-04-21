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
	std::array<char, KB(8)> buffer;

  public:
	Socket();
	virtual ~Socket();

	bool sendPacket(const Message::Packet &);
	bool connectWithAddress(const Address &, const bool isServer);

	virtual bool connect(const char *hostname, const char *port, const bool isServer) = 0;
	virtual bool send(const void *, size_t) = 0;
	virtual bool read(const int timeout, Bytes &) = 0;

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const = 0;
};
class TcpSocket : public Socket
{
  private:
	int fd;

	void close();

  public:
	TcpSocket();
	TcpSocket(int);
	TcpSocket(const TcpSocket &) = delete;
	TcpSocket(TcpSocket &&) noexcept;
	virtual ~TcpSocket();

	virtual bool connect(const char *hostname, const char *port, const bool isServer) override;
	virtual bool send(const void *, size_t) override;
	virtual bool read(const int timeout, Bytes &) override;

	virtual bool getIpAndPort(std::array<char, INET6_ADDRSTRLEN> &, uint16_t &) const override;
};
} // namespace TemStream