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
	if (mSocket == nullptr || !mSocket->read(timeout, bytes) || bytes.size() > maxMessageSize)
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
			if (nextMessageSize > maxMessageSize)
			{
				return false;
			}

			bytes.erase(bytes.begin(), bytes.begin() + sizeof(uint32_t));
		}

		if (*nextMessageSize == bytes.size())
		{
			Message::Packet packet;
			{
				MemoryStream m;
				m.write(bytes.data(), bytes.size());
				cereal::PortableBinaryInputArchive ar(m);

				ar(packet);
			}

			const bool result = handlePacket(std::move(packet));
			bytes.clear();
			nextMessageSize = std::nullopt;
			return result;
		}
		else if (*nextMessageSize < bytes.size())
		{
			MemoryStream m;
			m.write(bytes.data(), *nextMessageSize);
			cereal::PortableBinaryInputArchive ar(m);

			Message::Packet packet;
			ar(packet);

			const bool result = handlePacket(std::move(packet));
			bytes.erase(bytes.begin(), bytes.begin() + *nextMessageSize);
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