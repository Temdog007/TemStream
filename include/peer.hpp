#pragma once

#include <main.hpp>

namespace TemStream
{
enum PeerType : uint16_t
{
	Consumer,
	Producer,
	Server
};

struct PeerInformation
{
	std::string name;
	PeerType role;

	bool isConsumer() const
	{
		return (role & PeerType::Consumer) != 0;
	}

	bool isProducer() const
	{
		return (role & PeerType::Producer) != 0;
	}

	bool isServer() const
	{
		return (role & PeerType::Server) != 0;
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