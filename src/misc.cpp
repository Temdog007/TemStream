#include <main.hpp>

namespace TemStream
{
bool openSocket(int &fd, const char *hostname, const char *port, const bool isServer)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	AddrInfo info;
	if (!info.getInfo(hostname, port, hints))
	{
		return false;
	}

	if (!info.makeSocket(fd) || fd < 0)
	{
		perror("socket");
		return false;
	}

	if (isServer)
	{
		if (!info.bind(fd))
		{
			perror("bind");
			return false;
		}
		if (listen(fd, 10) < 0)
		{
			perror("listen");
			return false;
		}
	}
	else if (!info.connect(fd))
	{
		perror("connect");
		return false;
	}

	return true;
}

PollState pollSocket(const int fd, const int timeout)
{
	struct pollfd inputfd;
	inputfd.fd = fd;
	inputfd.events = POLLIN;
	inputfd.revents = 0;
	switch (poll(&inputfd, 1, timeout))
	{
	case -1:
		perror("poll");
		return PollState::Error;
	case 0:
		return PollState::NoData;
	default:
		return (inputfd.revents & POLLIN) == 0 ? PollState::NoData : PollState::GotData;
	}
}
} // namespace TemStream