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
	bool makeSocket(int &) const;
	bool bind(int) const;
	bool connect(int) const;

	struct addrinfo *getRes();
	const struct addrinfo *getRes() const;
};
} // namespace TemStream