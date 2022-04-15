#pragma once

#include <main.hpp>

namespace TemStream
{
class Producer : public Peer
{
  private:
	std::mutex mutex;

	bool handleData(const Bytes &) override;

  public:
	Producer();
	~Producer();

	bool init(const char *hostname, const char *port) override;

	template <class T> bool sendMessage(const T &t)
	{
		std::istringstream is;
		cereal::PortableBinaryInputArchive in(is);
		in(t);
		const std::string str(is.str());
		std::lock_guard<std::mutex> guard(mutex);
		return sendData(fd, reinterpret_cast<const uint8_t *>(str.data()), str.size());
	}
};
} // namespace TemStream