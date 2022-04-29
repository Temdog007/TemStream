#include <main.hpp>

namespace TemStream
{
Connection::Connection(const Address &address, unique_ptr<Socket> s)
	: bytes(), nextMessageSize(std::nullopt), info(), address(address), mSocket(std::move(s)), maxMessageSize(MB(1))
{
}

Connection::~Connection()
{
}

bool Connection::readAndHandle(const int timeout)
{
	if (mSocket == nullptr || !mSocket->read(timeout, bytes))
	{
		return false;
	}

	try
	{
		while (!appDone)
		{
			if (!nextMessageSize.has_value())
			{
				if (bytes.size() < sizeof(uint32_t))
				{
					return true;
				}

				uint32_t value;
				memcpy(&value, bytes.data(), sizeof(uint32_t));
				nextMessageSize.emplace(ntohl(value));
				if (*nextMessageSize > maxMessageSize)
				{
					(*logger)(Logger::Error) << "Got message larger than max acceptable size. Got " << *nextMessageSize
											 << "; Expected " << maxMessageSize << std::endl;
					return false;
				}

				bytes.remove(sizeof(uint32_t));
			}

			if (*nextMessageSize == bytes.size())
			{
				Message::Packet packet;
				{
					MemoryStream m;
					m.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}

				if (!handlePacket(std::move(packet)))
				{
					return false;
				}
				bytes.clear();
				nextMessageSize = std::nullopt;
				return true;
			}
			else if (*nextMessageSize < bytes.size())
			{
				Message::Packet packet;
				{
					MemoryStream m;
					m.write(reinterpret_cast<const char *>(bytes.data()), *nextMessageSize);
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}

				if (!handlePacket(std::move(packet)))
				{
					return false;
				}
				bytes.remove(*nextMessageSize);
				nextMessageSize = std::nullopt;
			}
			else
			{
				return true;
			}
		}
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Connection::readData: " << e.what() << std::endl;
	}
	return false;
}
} // namespace TemStream