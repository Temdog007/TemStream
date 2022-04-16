#include <main.hpp>

namespace TemStream
{
Peer::Peer(std::unique_ptr<Socket> &&s) : bytes(), nextMessageSize(std::nullopt), info(), mSocket(std::move(s))
{
}

Peer::~Peer()
{
}

bool Peer::readAndHandle(const int timeout)
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

			bytes.erase(bytes.begin(), bytes.begin() + sizeof(uint32_t));
		}

		if (*nextMessageSize == bytes.size())
		{
			MemoryStream m;
			m.write(bytes.data(), bytes.size());
			cereal::PortableBinaryInputArchive ar(m);

			MessagePacket packet;
			ar(packet);

			const bool result = handlePacket(packet);
			bytes.clear();
			nextMessageSize = std::nullopt;
			return result;
		}
		else if (*nextMessageSize < bytes.size())
		{
			MemoryStream m;
			m.write(bytes.data(), *nextMessageSize);
			cereal::PortableBinaryInputArchive ar(m);

			MessagePacket packet;
			ar(packet);

			const bool result = handlePacket(packet);
			bytes.erase(bytes.begin(), bytes.begin() + *nextMessageSize);
			nextMessageSize = std::nullopt;
			return result;
		}
		return true;
	}
	catch (const std::exception &e)
	{
		fprintf(stderr, "Peer::readData: %s\n", e.what());
		return false;
	}
}
} // namespace TemStream