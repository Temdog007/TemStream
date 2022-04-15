#pragma once

#include <main.hpp>

namespace TemStream
{
class Producer
{
  private:
	std::mutex mutex;
	int fd;

  public:
	Producer();
	~Producer();

	bool init(const char *hostname, const char *port);
};
} // namespace TemStream