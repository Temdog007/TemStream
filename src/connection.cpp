#include <main.hpp>

namespace TemStream
{
Connection::Connection(const Address &address, unique_ptr<Socket> s)
	: bytes(MB(1)), packets(), nextMessageSize(std::nullopt), info(), address(address), mSocket(std::move(s)),
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
				if (bytes.size() < sizeof(Message::Header) + 1)
				{
					return true;
				}

				Message::Header header;
				MemoryStream m;
				m.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
				{
					cereal::PortableBinaryInputArchive ar(m);
					ar(header);
				}
				if (header.size > maxMessageSize || header.size == 0 || header.id != Message::MagicGuid)
				{
					// Something is happening where the value is incorrect sometimes. Until it is determined why and
					// fixed, this error will be handled by clearing the bytes received and dropping packet(s).
					(*logger)(Logger::Warning) << "Got invalid message header: " << header << "; Max size allowed "
											   << maxMessageSize << "; Magic Guid: " << Message::MagicGuid << std::endl;
					bytes.clear();
					return true;
				}

				nextMessageSize = header.size;
				bytes.remove(m->getReadPoint());
			}

			if (*nextMessageSize == bytes.size())
			{
				Message::Packet packet;
				MemoryStream m;
				{

					m.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}
				if (static_cast<size_t>(m->getReadPoint()) != bytes.size())
				{
					(*logger)(Logger::Error) << "Expected to read " << m->getReadPoint() << "bytes. Read "
											 << bytes.size() << " bytes" << std::endl;
					return false;
				}

				packets.push(std::move(packet));
				bytes.clear();
				nextMessageSize = std::nullopt;
				return true;
			}
			else if (*nextMessageSize < bytes.size())
			{
				Message::Packet packet;
				MemoryStream m;
				{
					m.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}
				if (static_cast<size_t>(m->getReadPoint()) != *nextMessageSize)
				{
					(*logger)(Logger::Error) << "Expected to read " << m->getReadPoint() << "bytes. Read "
											 << bytes.size() << " bytes" << std::endl;
					return false;
				}

				packets.push(std::move(packet));
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