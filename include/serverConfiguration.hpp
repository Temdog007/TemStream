/******************************************************************************
	Copyright (C) 2022 by Temitope Alaga <temdog007@yaoo.com>
	This program is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.
	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.
	You should have received a copy of the GNU General Public License
	along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

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