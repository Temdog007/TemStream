#include <main.hpp>

namespace TemStream
{
Producer::Producer() : mutex(), fd(-1)
{
}

Producer::~Producer()
{
	::close(fd);
	fd = -1;
}

bool Producer::init(const char *hostname, const char *port)
{
	return openSocket(fd, hostname, port, false);
}
} // namespace TemStream