#pragma once

#include <main.hpp>

namespace TemStream
{
class Configuration
{
  public:
	const char *hostname;
	int port;
	const char *serverName;
	int senders;
};
} // namespace TemStream