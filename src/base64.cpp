#include <main.hpp>

// Source: https://renenyffenegger.ch/notes/development/Base64/Encoding-and-decoding-base-64-with-cpp/

namespace TemStream
{
const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
						   "abcdefghijklmnopqrstuvwxyz"
						   "0123456789+/";

bool isBase64(char c)
{
	return isalnum(c) || c == '+' || c == '/';
}

const char trailingChar = '=';

String base64_encode(const ByteList &bytes)
{
	if (bytes.empty())
	{
		return String();
	}
	const size_t len_encoded = (bytes.size() + 2) / 3 * 4;

	String ret;
	ret.reserve(len_encoded);

	for (size_t pos = 0; pos < bytes.size(); pos += 3)
	{
		ret.push_back(base64Chars[(bytes[pos] & 0xfc) >> 2]);
		if (pos + 1 < bytes.size())
		{
			ret.push_back(base64Chars[((bytes[pos] & 0x03) << 4) + ((bytes[pos + 1] & 0xf0) >> 4)]);
			if (pos + 2 < bytes.size())
			{
				ret.push_back(base64Chars[((bytes[pos + 1] & 0x0f) << 2) + ((bytes[pos + 2] & 0xc0) >> 6)]);
				ret.push_back(base64Chars[bytes[pos + 2] & 0x3f]);
			}
			else
			{
				ret.push_back(base64Chars[(bytes[pos + 1] & 0x0f) << 2]);
				ret.push_back(trailingChar);
			}
		}
		else
		{
			ret.push_back(base64Chars[(bytes[pos] & 0x03) << 4]);
			ret.push_back(trailingChar);
			ret.push_back(trailingChar);
		}
	}
	return ret;
}

static unsigned int pos_of_char(const unsigned char chr)
{
	//
	// Return the position of chr within base64_encode()
	//

	if (chr >= 'A' && chr <= 'Z')
	{
		return chr - 'A';
	}
	else if (chr >= 'a' && chr <= 'z')
	{
		return chr - 'a' + ('Z' - 'A') + 1;
	}
	else if (chr >= '0' && chr <= '9')
	{
		return chr - '0' + ('Z' - 'A') + ('z' - 'a') + 2;
	}
	else if (chr == '+')
	{
		return 62;
	} // Be liberal with input and accept non-url ('+') base 64 characters
	else if (chr == '/')
	{
		return 63;
	}
	else
	{
		//
		// 2020-10-23: Throw std::exception rather than const char*
		//(Pablo Martin-Gomez, https://github.com/Bouska)
		//
		throw std::runtime_error("Input is not valid base64-encoded data.");
	}
}

ByteList base64_decode(const String &str)
{
	if (str.empty())
	{
		return ByteList();
	}

	ByteList ret(str.size() / 4 * 3);
	for (size_t pos = 0; pos < str.size(); pos += 4)
	{
		const auto c1 = pos_of_char(str[pos + 1]);
		ret.append((pos_of_char(str[pos]) << 2) + ((c1 & 0x30) >> 4));
		if (pos + 2 < str.size() && str[pos + 2] != trailingChar)
		{
			const auto c2 = pos_of_char(str[pos + 2]);
			ret.append(((c1 & 0x0f) << 4) + ((c2 & 0x3c) >> 2));
			if (pos + 3 < str.size() && str[pos + 3] != trailingChar)
			{
				ret.append(((c2 & 0x03) << 6) + pos_of_char(str[pos + 3]));
			}
		}
	}
	return ret;
}
} // namespace TemStream