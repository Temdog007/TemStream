#pragma once

#include <main.hpp>

namespace TemStream
{
struct Address
{
	std::string hostname;
	int port;

	Address() : hostname("localhost"), port(DefaultPort)
	{
	}
	Address(const char *hostname, int port) : hostname(hostname), port(port)
	{
	}
	~Address()
	{
	}

	bool operator==(const Address &a) const
	{
		return port == a.port && hostname == a.hostname;
	}
	bool operator!=(const Address &a) const
	{
		return !(*this == a);
	}

	std::unique_ptr<TcpSocket> makeTcpSocket() const
	{
		auto ptr = std::make_unique<TcpSocket>();
		if (ptr->connectWithAddress(*this, false))
		{
			return ptr;
		}
		return nullptr;
	}

	friend std::ostream &operator<<(std::ostream &os, const Address &a)
	{
		os << a.hostname << ':' << a.port;
		return os;
	}
};
} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::Address>
{
	std::size_t operator()(const TemStream::Address &addr) const
	{
		return hash<string>()(addr.hostname) ^ hash<int>()(addr.port);
	}
};
} // namespace std