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