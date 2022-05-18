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
/**
 * @brief Address to a server
 * @tparam S string type
 */
template <class S> struct BaseAddress
{
	S hostname;
	int port;

	BaseAddress() : hostname("localhost"), port(10000)
	{
	}
	template <class U> BaseAddress(const BaseAddress<U> &a) : hostname(a.hostname), port(a.port)
	{
	}
	template <class U> BaseAddress(BaseAddress<U> &&a) : hostname(std::move(a.hostname)), port(a.port)
	{
	}
	BaseAddress(const char *hostname, int port) : hostname(hostname), port(port)
	{
	}
	~BaseAddress()
	{
	}

	/**
	 * Save for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void save(Archive &archive) const
	{
		std::string h(hostname);
		archive(cereal::make_nvp("hostname", h), cereal::make_nvp("port", port));
	}

	/**
	 * Loading for cereal serialization
	 *
	 * @param ar The archive
	 */
	template <class Archive> void load(Archive &archive)
	{
		std::string h(hostname);
		archive(cereal::make_nvp("hostname", h), cereal::make_nvp("port", port));
		hostname = h;
	}

	bool operator==(const BaseAddress &a) const
	{
		return port == a.port && hostname == a.hostname;
	}
	bool operator!=(const BaseAddress &a) const
	{
		return !(*this == a);
	}

	friend std::ostream &operator<<(std::ostream &os, const BaseAddress &a)
	{
		os << a.hostname << ':' << a.port;
		return os;
	}

	/**
	 * @brief Creates a connection with this address
	 * @tparam T socket type (i.e. UpSocket, TcpSocket)
	 *
	 * @return This pointer to a socket connection or nullptr
	 */
	template <class T> unique_ptr<T> create() const
	{
		auto ptr = tem_unique<T>();
		char portStr[64];
		snprintf(portStr, sizeof(portStr), "%d", port);
		if (ptr->connect(hostname.c_str(), portStr))
		{
			return ptr;
		}
		return nullptr;
	}
};
using Address = BaseAddress<String>;
using STL_Address = BaseAddress<std::string>;

/**
 * @brief Open a socket with these parameters
 *
 * @param socket [out] Will contain the newly created socket on success
 * @param address
 * @param socketType
 * @param isTcp If true, establish a TCP connection. Else, UDP.
 *
 * @return True if successful
 */
extern bool openSocket(SOCKET &socket, const Address &address, const SocketType socketType, const bool isTcp);

/**
 * @brief Open a socket with these parameters
 * @tparam T socket type (i.e. UpSocket, TcpSocket)
 *
 * @param address
 * @param socketType
 * @param isTcp If true, establish a TCP connection. Else, UDP.
 *
 * @return This pointer to a socket connection or nullptr
 */
template <typename T> unique_ptr<T> openSocket(const Address &address, const SocketType socketType, const bool isTcp)
{
	SOCKET fd = INVALID_SOCKET;
	if (!openSocket(fd, address, socketType, isTcp))
	{
		return nullptr;
	}

	return tem_unique<T>(fd);
}

} // namespace TemStream

namespace std
{
template <> struct hash<TemStream::Address>
{
	std::size_t operator()(const TemStream::Address &addr) const
	{
		std::size_t value = hash<TemStream::String>()(addr.hostname);
		TemStream::hash_combine(value, addr.port);
		return value;
	}
};
} // namespace std