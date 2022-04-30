#include <main.hpp>

#include <netinet/tcp.h>

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
		(*logger)(Logger::Error) << "getaddrinfo: " << gai_strerror(result) << std::endl;
		return false;
	}
	return true;
}

bool AddrInfo::makeSocket(int &sockfd) const
{
	if (res == nullptr)
	{
		return false;
	}
	int yes = 1;
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0)
	{
		perror("setsockopt");
		return false;
	}
	yes = 1;
	if (setsockopt(sockfd, SOL_TCP, TCP_NODELAY, &yes, sizeof(yes)) < 0)
	{
		perror("setsockopt");
		return false;
	}
	return true;
}

bool AddrInfo::bind(const int sockfd) const
{
	if (res == nullptr)
	{
		return false;
	}
	if (::bind(sockfd, res->ai_addr, res->ai_addrlen) < 0)
	{
		perror("bind");
		return false;
	}
	return true;
}

bool AddrInfo::connect(const int sockfd) const
{
	if (res == nullptr)
	{
		return false;
	}
	if (::connect(sockfd, res->ai_addr, res->ai_addrlen) < 0)
	{
		perror("connect");
		return false;
	}
	return true;
}
} // namespace TemStream