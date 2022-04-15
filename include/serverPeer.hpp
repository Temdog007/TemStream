#pragma once

#include <main.hpp>

namespace TemStream
{
extern int runServer(int, const char **);

class ServerPeer : public Peer
{
  public:
	ServerPeer(int);
	virtual ~ServerPeer();

	virtual bool operator()(const TextMessage &);
	virtual bool operator()(const ImageMessage &);
	virtual bool operator()(const VideoMessage &);
	virtual bool operator()(const AudioMessage &);
	virtual bool operator()(const PeerInformationList &);
};
} // namespace TemStream