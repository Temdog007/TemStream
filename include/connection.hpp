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
class Connection
{
  private:
	ByteList bytes;
	ConcurrentQueue<Message::Packet> packets;
	std::optional<uint64_t> nextMessageSize;

  protected:
	const Address address;
	unique_ptr<Socket> mSocket;
	uint64_t maxMessageSize;

  public:
	Connection(const Address &, unique_ptr<Socket>);
	Connection(const Connection &) = delete;
	Connection(Connection &&) = delete;
	virtual ~Connection();

	const Address &getAddress() const
	{
		return address;
	}

	Socket *operator->()
	{
		return mSocket.get();
	}

	/**
	 * Get packets that were received from the connected peer
	 *
	 * @return the packets
	 */
	ConcurrentQueue<Message::Packet> &getPackets()
	{
		return packets;
	}

	bool readAndHandle(const int);
};

} // namespace TemStream