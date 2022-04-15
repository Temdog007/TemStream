#include <main.hpp>

AddrInfo::AddrInfo() : res(NULL)
{
}

AddrInfo::~AddrInfo()
{
	close();
}

void AddrInfo::close()
{
	if (res != NULL)
	{
		freeaddrinfo(res);
		res = NULL;
	}
}

bool AddrInfo::getInfo(const char *hostname, const char *port, const struct addrinfo &hints)
{
	close();

	const int result = getaddrinfo(hostname, port, &hints, &res);
	if (result != 0 || res == NULL)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(result));
		return false;
	}
	return true;
}

bool AddrInfo::makeSocket(int &sockfd) const
{
	if (res == NULL)
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
	return true;
}

bool AddrInfo::bind(const int sockfd) const
{
	if (res == NULL)
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
	if (res == NULL)
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
