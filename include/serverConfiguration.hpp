#pragma once

#include <main.hpp>

namespace TemStream
{
struct SSLConfig
{
	String key;
	String cert;
};
typedef bool (*VerifyToken)(const char *, char (&username)[32], uint32_t &);
typedef bool (*VerifyUsernameAndPassword)(const char *, const char *, uint32_t &);
struct Configuration
{
	Access access;
	Address address;
	String name;
	std::optional<SSLConfig> ssl;
	int64_t startTime;
#if __unix__
	void *handle;
#else
	HMODULE handle;
#endif
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

	Message::Source getSource() const;
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