#pragma once

#include <main.hpp>

namespace TemStream
{
template <class S> struct BaseAddress
{
	S hostname;
	int port;

	BaseAddress() : hostname("localhost"), port(10000)
	{
	}
	template <class U> BaseAddress(const BaseAddress<U> &a) : hostname(a.hostname), port(a.port)
	{
	}
	template <class U> BaseAddress(BaseAddress<U> &&a) : hostname(std::move(a.hostname)), port(a.port)
	{
	}
	BaseAddress(const char *hostname, int port) : hostname(hostname), port(port)
	{
	}
	~BaseAddress()
	{
	}

	template <class Archive> void save(Archive &archive) const
	{
		std::string h(hostname);
		archive(cereal::make_nvp("hostname", h), cereal::make_nvp("port", port));
	}

	template <class Archive> void load(Archive &archive)
	{
		std::string h(hostname);
		archive(cereal::make_nvp("hostname", h), cereal::make_nvp("port", port));
		hostname = h;
	}

	bool operator==(const BaseAddress &a) const
	{
		return port == a.port && hostname == a.hostname;
	}
	bool operator!=(const BaseAddress &a) const
	{
		return !(*this == a);
	}

	friend std::ostream &operator<<(std::ostream &os, const BaseAddress &a)
	{
		os << a.hostname << ':' << a.port;
		return os;
	}

	template <class T> unique_ptr<T> create(bool isServer) const
	{
		auto ptr = tem_unique<T>();
		char portStr[64];
		snprintf(portStr, sizeof(portStr), "%d", port);
		if (ptr->connect(hostname.c_str(), portStr, isServer))
		{
			return ptr;
		}
		return nullptr;
	}
};
using Address = BaseAddress<String>;
using STL_Address = BaseAddress<std::string>;
extern bool openSocket(int &, const Address &, const bool isServer, const bool isTcp);

template <typename T> unique_ptr<T> openSocket(const Address &address, const bool isServer, const bool isTcp)
{
	int fd = -1;
	if (!openSocket(fd, address, isServer, isTcp))
	{
		return nullptr;
	}

	return tem_unique<T>(fd);
}
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