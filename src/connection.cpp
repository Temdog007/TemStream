#include <main.hpp>

namespace TemStream
{
Connection::Connection(const Address &address, unique_ptr<Socket> s)
	: bytes(MB(1)), nextMessageSize(std::nullopt), info(), address(address), mSocket(std::move(s)),
	  maxMessageSize(MB(1))
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

				uint32_t value = 0;
				memcpy(&value, bytes.data(), sizeof(uint32_t));
				bytes.remove(sizeof(uint32_t));
				value = ntohl(value);
				if (value > maxMessageSize || value == 0)
				{
					// Something is happening where the value is incorrect sometimes. Until it is determined why and
					// fixed, this error will be handled by clearing the bytes received and dropping packet(s).
					(*logger)(Logger::Warning) << "Got message with unacceptable size. Got " << value
											   << "; Max size allowed " << maxMessageSize << std::endl;
					bytes.clear();
					return true;
				}

				nextMessageSize = value;
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