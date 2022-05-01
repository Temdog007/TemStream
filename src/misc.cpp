#include <main.hpp>

namespace TemStream
{
namespace Message
{
std::ostream &operator<<(std::ostream &os, const Header &header)
{
	os << "ID: " << header.id << "; Size: " << header.size;
	return os;
}
const Guid MagicGuid(0x2abe3059992u, 0xa589a5bbc5u);
void prepareImageBytes(std::ifstream &file, const Source &source, const std::function<void(Packet &&)> &func)
{
	auto size = file.tellg();
	file.seekg(0, std::ios::end);

	size = file.tellg() - size;

	file.seekg(0, std::ios::beg);

	prepareImageBytes(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), size, source, func);
}
} // namespace Message
const char *getExtension(const char *filename)
{
	size_t len = strlen(filename);
	const char *c = filename + (len - 1);
	for (; *c != *filename; --c)
	{
		if (*c == '.')
		{
			return c + 1;
		}
	}
	return filename;
}
std::ostream &printMemory(std::ostream &os, const char *label, const size_t mem)
{
	os << label << ": " << printMemory(mem);
	return os;
}

String printMemory(const size_t mem)
{
	char buffer[KB(1)];
	if (mem >= GB(1))
	{
		snprintf(buffer, sizeof(buffer), "%3.2f GB", (float)mem / (float)(GB(1)));
	}
	else if (mem >= MB(1))
	{
		snprintf(buffer, sizeof(buffer), "%3.2f MB", (float)mem / (float)(MB(1)));
	}
	else if (mem >= KB(1))
	{
		snprintf(buffer, sizeof(buffer), "%3.2f KB", (float)mem / (float)(KB(1)));
	}
	else
	{
		snprintf(buffer, sizeof(buffer), "%zu bytes", mem);
	}
	return String(buffer);
}

bool openSocket(int &fd, const Address &address, const bool isServer)
{
	char port[64];
	snprintf(port, sizeof(port), "%d", address.port);
	return openSocket(fd, address.hostname.c_str(), port, isServer);
}

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

int64_t getTimestamp()
{
	auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

PollState pollSocket(const int fd, const int timeout, const int events)
{
	struct pollfd inputfd;
	inputfd.fd = fd;
	inputfd.events = events;
	inputfd.revents = 0;
	switch (poll(&inputfd, 1, timeout))
	{
	case -1:
		perror("poll");
		return PollState::Error;
	case 0:
		return PollState::NoData;
	default:
		return (inputfd.revents & events) == 0 ? PollState::NoData : PollState::GotData;
	}
}
String &trim(String &s)
{
	return ltrim(rtrim(s));
}
String &ltrim(String &s)
{
	auto end = std::find_if(s.begin(), s.end(), [](char c) { return !std::isspace(c); });
	s.erase(s.begin(), end);
	return s;
}
String &rtrim(String &s)
{
	auto start = std::find_if(s.rbegin(), s.rend(), [](char c) { return !std::isspace(c); });
	s.erase(start.base(), s.end());
	return s;
}
} // namespace TemStream
