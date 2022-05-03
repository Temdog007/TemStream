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

bool Connection::readAndHandle(const int timeout, const bool base64)
{
	if (mSocket == nullptr || !mSocket->read(timeout, bytes))
	{
		return false;
	}

	try
	{
		if (base64)
		{
			while (true)
			{
				// Look for null terminator
				ptrdiff_t i = bytes.size() - 1;
				for (; i >= 0; --i)
				{
					if (bytes[i] == '\0')
					{
						break;
					}
				}
				if (i < 0)
				{
					break;
				}

				Message::Packet packet;
				MemoryStream m;
				ByteList packetBytes(bytes.data(), i);
				packetBytes = base64_decode(packetBytes);
				{
					m.write(reinterpret_cast<const char *>(packetBytes.data()), packetBytes.size());
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}
				if (m->getReadPoint() != packetBytes.size<ssize_t>())
				{
					(*logger)(Logger::Error)
						<< "Expected to read " << m->getReadPoint() << "bytes. Read " << i << " bytes" << std::endl;
					return false;
				}

				packets.push(std::move(packet));
				// Remove the null terminator also
				bytes.remove(i + 1);
			}
			return true;
		}
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
#if _DEBUG
					(*logger)(Logger::Error) << "Got invalid message header: " << header << "; Max size allowed "
											 << maxMessageSize << "; Magic Guid: " << Message::MagicGuid << std::endl;
#else
					(*logger)(Logger::Error) << "Got invalid message header: " << header.size << "; Max size allowed "
											 << maxMessageSize << std::endl;
#endif
					return false;
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
	catch (const std::bad_alloc &)
	{
		(*logger)(Logger::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Error) << "Connection::readData: " << e.what() << std::endl;
	}
	return false;
}
} // namespace TemStream