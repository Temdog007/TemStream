#include <main.hpp>

namespace TemStream
{
Connection::Connection(const Address &address, unique_ptr<Socket> s)
	: bytes(MB(1)), packets(), nextMessageSize(std::nullopt), address(address), mSocket(std::move(s)),
	  maxMessageSize(MB(1))
{
}

Connection::~Connection()
{
}

bool Connection::readAndHandle(const int timeout)
{
	if (mSocket == nullptr || !mSocket->read(timeout, bytes, true))
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
				MemoryStream m(std::move(bytes));
				{
					cereal::PortableBinaryInputArchive ar(m);
					ar(header);
				}
				if (header.size > maxMessageSize || header.size == 0 || header.id != Message::MagicGuid)
				{
#if _DEBUG
					(*logger)(Logger::Level::Error)
						<< "Got invalid message header: " << header << "; Max size allowed " << maxMessageSize
						<< "; Magic Guid: " << Message::MagicGuid << std::endl;
#else
					(*logger)(Logger::Level::Error) << "Got invalid message header: " << header.size
													<< "; Max size allowed " << maxMessageSize << std::endl;
#endif
					return false;
				}

				nextMessageSize = header.size;
				const auto read = static_cast<uint32_t>(m->getReadPoint());
				bytes = std::move(m->moveBytes());
				bytes.remove(read);
			}

			if (*nextMessageSize == bytes.size())
			{
				Message::Packet packet;
				MemoryStream m(std::move(bytes));
				{
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}
				const auto &tempBytes = m->getBytes();
				if (static_cast<size_t>(m->getReadPoint()) != tempBytes.size())
				{
					(*logger)(Logger::Level::Error) << "Expected to read " << m->getReadPoint() << "bytes. Read "
													<< tempBytes.size() << " bytes" << std::endl;
					return false;
				}

				packets.push(std::move(packet));
				nextMessageSize = std::nullopt;
				return true;
			}
			else if (*nextMessageSize < bytes.size())
			{
				Message::Packet packet;
				MemoryStream m(std::move(bytes));
				{
					cereal::PortableBinaryInputArchive ar(m);
					ar(packet);
				}
				if (static_cast<size_t>(m->getReadPoint()) != *nextMessageSize)
				{
					(*logger)(Logger::Level::Error) << "Expected to read " << m->getReadPoint() << "bytes. Read "
													<< bytes.size() << " bytes" << std::endl;
					return false;
				}

				packets.push(std::move(packet));
				bytes = std::move(m->moveBytes());
				bytes.remove(static_cast<uint32_t>(*nextMessageSize));
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
		(*logger)(Logger::Level::Error) << "Ran out of memory" << std::endl;
	}
	catch (const std::exception &e)
	{
		(*logger)(Logger::Level::Error) << "Connection::readAndHandle: " << e.what() << std::endl;
	}
	return false;
}
} // namespace TemStream