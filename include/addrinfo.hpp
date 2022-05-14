#pragma once

#include <main.hpp>

namespace TemStream
{
class AddrInfo
{
  private:
	struct addrinfo *res;
	void close();

  public:
	AddrInfo();
	~AddrInfo();

	bool getInfo(const char *hostname, const char *port, const struct addrinfo &);
	bool makeSocket(SOCKET &, const bool isTcp) const;
	bool bind(SOCKET) const;
	bool connect(SOCKET) const;

	struct addrinfo *getRes();
	const struct addrinfo *getRes() const;
};
} // namespace TemStream