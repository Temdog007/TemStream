#include <main.hpp>

#if __linux__
#include <netinet/tcp.h>
#endif

namespace TemStream
{
AddrInfo::AddrInfo() : res(nullptr)
{
}

AddrInfo::~AddrInfo()
{
	close();
}

void AddrInfo::close()
{
	if (res != nullptr)
	{
		freeaddrinfo(res);
		res = nullptr;
	}
}

bool AddrInfo::getInfo(const char *hostname, const char *port, const struct addrinfo &hints)
{
	close();

	const int result = getaddrinfo(hostname, port, &hints, &res);
	if (result != 0 || res == nullptr)
	{
		(*logger)(Logger::Level::Error) << "getaddrinfo: " << gai_strerror(result) << std::endl;
		return false;
	}
	return true;
}

bool AddrInfo::makeSocket(SOCKET &sockfd, const bool isTcp) const
{
	if (res == nullptr)
	{
		return false;
	}
	int yes = 1;
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0)
	{
		perror("setsockopt");
		return false;
	}
	yes = 1;
	if (isTcp && setsockopt(sockfd, SOL_TCP, TCP_NODELAY, reinterpret_cast<const char *>(&yes), sizeof(yes)) < 0)
	{
		perror("setsockopt");
		return false;
	}
	return true;
}

bool AddrInfo::bind(const SOCKET sockfd) const
{
	if (res == nullptr)
	{
		return false;
	}
	if (::bind(sockfd, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) < 0)
	{
		perror("bind");
		return false;
	}
	return true;
}

bool AddrInfo::connect(const SOCKET sockfd) const
{
	if (res == nullptr)
	{
		return false;
	}
	if (::connect(sockfd, res->ai_addr, static_cast<socklen_t>(res->ai_addrlen)) < 0)
	{
		perror("connect");
		return false;
	}
	return true;
}
} // namespace TemStream