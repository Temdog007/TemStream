#pragma once

#include <main.hpp>

namespace TemStream
{
enum PeerRole : uint16_t
{
	Consumer,
	Producer,
	Server
};

struct PeerInformation
{
	std::string name;
	PeerRole role;

	bool isConsumer() const
	{
		return (role & PeerRole::Consumer) != 0;
	}

	bool isProducer() const
	{
		return (role & PeerRole::Producer) != 0;
	}

	bool isServer() const
	{
		return (role & PeerRole::Server) != 0;
	}

	template <class Archive> void serialize(Archive &archive)
	{
		archive(name, role);
	}
};

class Peer
{
  private:
	Bytes data;
	PeerInformation info;
	std::optional<uint32_t> nextMessageSize;

	void close();

	virtual bool handleData(const Bytes &) = 0;

  protected:
	int fd;
	PeerInformation &getInfo()
	{
		return info;
	}

  public:
	Peer();
	virtual ~Peer();

	virtual bool init(const char *hostname, const char *port) = 0;

	const PeerInformation &getInfo() const
	{
		return info;
	}

	int getFd() const
	{
		return fd;
	}

	bool readData();
};

} // namespace TemStream