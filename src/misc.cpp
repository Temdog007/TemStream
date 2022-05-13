#include <main.hpp>

namespace TemStream
{
const char *ServerTypeStrings[ServerTypeCount()];
std::ostream &operator<<(std::ostream &os, const ServerType type)
{
	static bool first = true;
	if (first)
	{
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Link);
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Text);
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Chat);
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Image);
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Audio);
		WRITE_STRING_TO_STRING_ARRAY(ServerType, Video);
		first = false;
	}
	if (validServerType(type))
	{
		os << ServerTypeStrings[(uint32_t)type];
	}
	else
	{
		os << "Unknown";
	}
	return os;
}

std::ostream &operator<<(std::ostream &os, const PeerFlags flags)
{
	os << '[';
	if (peerFlagsOverlap(flags, PeerFlags::WriteAccess))
	{
		os << "WriteAcess,";
	}
	if (peerFlagsOverlap(flags, PeerFlags::ReplayAccess))
	{
		os << "ReplayAcess,";
	}
	if (peerFlagsOverlap(flags, PeerFlags::Moderator))
	{
		os << "Moderator,";
	}
	if (peerFlagsOverlap(flags, PeerFlags::Owner))
	{
		os << "Owner";
	}
	os << ']';
	return os;
}
namespace Message
{
std::ostream &operator<<(std::ostream &os, const Message::Packet &packet)
{
	ByteList bytes;
	{
		MemoryStream m;
		{
			cereal::PortableBinaryOutputArchive ar(m);
			ar(packet);
		}
		bytes = ByteList(std::move(m));
	}
	const String str = base64_encode(bytes);
	os << str;
	return os;
}
std::ostream &operator<<(std::ostream &os, const Header &header)
{
	os << "ID: " << header.id << "; Size: " << header.size;
	return os;
}
std::ostream &operator<<(std::ostream &os, const TimeRange &t)
{
	os << t.end - t.start << " seconds";
	return os;
}
const Guid MagicGuid(0x2abe3059992u, 0xa589a5bbc5u);
void prepareLargeBytes(std::ifstream &file, const std::function<void(LargeFile &&)> &func)
{
	auto size = file.tellg();
	file.seekg(0, std::ios::end);

	size = file.tellg() - size;

	file.seekg(0, std::ios::beg);

	prepareLargeBytes(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>(), size, func);
}
void prepareLargeBytes(const ByteList &bytes, const std::function<void(LargeFile &&)> &func)
{
	{
		LargeFile lf = bytes.size<uint64_t>();
		func(std::move(lf));
	}
	for (size_t i = 0; i < bytes.size(); i += MAX_FILE_CHUNK)
	{
		LargeFile lf = ByteList(bytes, MAX_FILE_CHUNK, i);
		func(std::move(lf));
	}
	{
		LargeFile lf = std::monostate{};
		func(std::move(lf));
	}
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

bool openSocket(int &fd, const Address &address, const SocketType t, const bool isTcp)
{
	char port[64];
	snprintf(port, sizeof(port), "%d", address.port);
	return openSocket(fd, address.hostname.c_str(), port, t, isTcp);
}

bool openSocket(int &fd, const char *hostname, const char *port, const SocketType t, const bool isTcp)
{
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = isTcp ? SOCK_STREAM : SOCK_DGRAM;

	AddrInfo info;
	if (!info.getInfo(hostname, port, hints))
	{
		return false;
	}

	if (!info.makeSocket(fd, isTcp) || fd < 0)
	{
		perror("socket");
		return false;
	}

	switch (t)
	{
	case SocketType::Server:
		if (!info.bind(fd))
		{
			perror("bind");
			return false;
		}
		if (isTcp && listen(fd, 10) < 0)
		{
			perror("listen");
			return false;
		}
		break;
	case SocketType::Client:
		if (!info.connect(fd))
		{
			perror("connect");
			return false;
		}
		break;
	default:
		break;
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
