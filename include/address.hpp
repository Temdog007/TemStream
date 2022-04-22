#pragma once

#include <main.hpp>

namespace TemStream
{
struct Address
{
	String hostname;
	int port;

	Address() : hostname("localhost"), port(10000)
	{
	}
	Address(const char *hostname, int port) : hostname(hostname), port(port)
	{
	}
	~Address()
	{
	}

	template <class Archive> void save(Archive &archive) const
	{
		archive(hostname, port);
	}

	template <class Archive> void load(Archive &archive)
	{
		archive(hostname, port);
	}

	bool operator==(const Address &a) const
	{
		return port == a.port && hostname == a.hostname;
	}
	bool operator!=(const Address &a) const
	{
		return !(*this == a);
	}

	unique_ptr<TcpSocket> makeTcpSocket() const
	{
		auto ptr = tem_unique<TcpSocket>();
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
extern bool openSocket(int &, const Address &, const bool isServer);
} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::Address>
{
	std::size_t operator()(const TemStream::Address &addr) const
	{
		std::size_t value = hash<TemStream::String>()(addr.hostname);
		TemStream::hash_combine(value, addr.port);
		return value;
	}
};
} // namespace std