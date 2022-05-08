#pragma once

#include <main.hpp>

namespace TemStream
{

typedef bool (*VerifyToken)(const char *, char *, uint32_t *);
typedef bool (*VerifyUsernameAndPassword)(const char *, const char *, uint32_t *);
struct Configuration
{
	Access access;
	Address address;
	String name;
	int64_t startTime;
	void *handle;
	VerifyToken verifyToken;
	VerifyUsernameAndPassword verifyUsernameAndPassword;
	uint32_t messageRateInSeconds;
	uint32_t maxClients;
	uint32_t maxMessageSize;
	ServerType serverType;
	bool record;

	Configuration();
	~Configuration();

	bool valid() const;
};
extern std::ostream &operator<<(std::ostream &, const Configuration &);
class CredentialHandler
{
  private:
	VerifyToken verifyToken;
	VerifyUsernameAndPassword verifyUsernameAndPassword;

  public:
	CredentialHandler(VerifyToken, VerifyUsernameAndPassword);
	CredentialHandler(const Configuration &);
	~CredentialHandler();

	std::optional<PeerInformation> operator()(const String &);
	std::optional<PeerInformation> operator()(const Message::UsernameAndPassword &);
};
} // namespace TemStream