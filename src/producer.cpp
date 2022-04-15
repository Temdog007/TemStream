#include <main.hpp>

namespace TemStream
{
Producer::Producer() : Peer(), mutex()
{
}

Producer::~Producer()
{
}

bool Producer::init(const char *hostname, const char *port)
{
	return openSocket(fd, hostname, port, false);
}

bool Producer::handleData(const Bytes &)
{
	fprintf(stderr, "Producer::handleData Producer does not handle data from other peers!\n");
	return false;
}
} // namespace TemStream