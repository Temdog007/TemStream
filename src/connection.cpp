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
		if (!nextMessageSize.has_value())
		{
			if (bytes.size() < sizeof(uint32_t))
			{
				return true;
			}

			union {
				uint32_t value;
				char data[4];
			} v;

			for (size_t i = 0; i < 4; ++i)
			{
				v.data[i] = bytes[i];
			}
			nextMessageSize = ntohl(v.value);
			if (nextMessageSize > maxMessageSize || nextMessageSize == 0)
			{
				(*logger)(Logger::Error) << "Got message larger than max acceptable size. Got " << *nextMessageSize
										 << "; Expected " << maxMessageSize << std::endl;
				return false;
			}

			bytes.remove(sizeof(uint32_t));
		}

		if (nextMessageSize == bytes.size())
		{
			Message::Packet packet;
			{
				MemoryStream m;
				m.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
				cereal::PortableBinaryInputArchive ar(m);

				ar(packet);
			}

			const bool result = handlePacket(std::move(packet));
			bytes.clear();
			nextMessageSize = std::nullopt;
			return result;
		}
		else if (nextMessageSize < bytes.size())
		{
			MemoryStream m;
			m.write(reinterpret_cast<const char *>(bytes.data()), *nextMessageSize);
			cereal::PortableBinaryInputArchive ar(m);

			Message::Packet packet;
			ar(packet);

			const bool result = handlePacket(std::move(packet));
			bytes.remove(*nextMessageSize);
			nextMessageSize = std::nullopt;
			return result;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Connection::readData: " << e.what() << std::endl;
		return false;
	}
}
} // namespace TemStream