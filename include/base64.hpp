#pragma once

#include <main.hpp>

namespace TemStream
{
extern const char base64Chars[];

extern bool isBase64(char);

extern String base64_encode(const ByteList &);
extern ByteList base64_decode(const String &);
} // namespace TemStream