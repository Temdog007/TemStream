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
 * @brief Wrapper for struct addrinfo
 */
class AddrInfo
{
  private:
	struct addrinfo *res;
	void close();

  public:
	AddrInfo();
	~AddrInfo();

	/**
	 * @brief Store addrinfo from parameters.
	 *
	 * This closes the pointer to the previous addrinfo and tries to get the new addrinfo
	 * from the parameters
	 *
	 * @param hostname
	 * @param port
	 * @param info
	 *
	 * @return True if addrinfo was acquired.
	 */
	bool getInfo(const char *hostname, const char *port, const struct addrinfo &info);

	/**
	 * @brief Create socket from addrinfo
	 *
	 * @param socket [out] Will contain the new socket
	 * @param isTcp If true, establish a TCP connection. Else, UDP.
	 *
	 * @return True if socket was created.
	 */
	bool makeSocket(SOCKET &socket, const bool isTcp) const;

	/**
	 * @brief Bind socket using addrinfo
	 *
	 * @param socket
	 *
	 * @return True if successful
	 */
	bool bind(SOCKET socket) const;

	/**
	 * @brief Connect socket using addrinfo
	 *
	 * @param socket
	 *
	 * @return True if successful
	 */
	bool connect(SOCKET socket) const;
};
} // namespace TemStream