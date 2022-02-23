#include <include/main.h>

bool
parseAddress(const char* str, pAddress address)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        address->tag = AddressTag_ipAddress;
        address->ipAddress.ip =
          TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        address->ipAddress.port =
          TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    address->tag = AddressTag_domainSocket;
    address->domainSocket = TemLangStringCreate(str, currentAllocator);
    return true;
}

bool
parseCredentials(const char* str, pCredentials c)
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; ++i) {
        if (str[i] != ':') {
            continue;
        }

        c->username = TemLangStringCreateFromSize(str, i + 1, currentAllocator);
        c->password = TemLangStringCreate(str + i + 1, currentAllocator);
        return true;
    }
    return false;
}

TemLangString
RandomClientName(pRandomState rs)
{
    TemLangString name = RandomString(rs, 3, 10);
    TemLangStringInsertChar(&name, '@', 0);
    return name;
}

TemLangString
RandomString(pRandomState rs, const uint64_t min, const uint64_t max)
{
    const uint64_t len = min + (random64(rs) % (max - min));
    TemLangString name = { .allocator = currentAllocator,
                           .buffer = currentAllocator->allocate(len),
                           .size = len,
                           .used = 0 };
    for (uint64_t i = 0; i < len; ++i) {
        do {
            const char c = (char)(random64(rs) % 128ULL);
            switch (c) {
                case ':':
                    continue;
                case '_':
                case ' ':
                    break;
                default:
                    if (isalnum(c)) {
                        break;
                    }
                    continue;
            }
            TemLangStringAppendChar(&name, c);
            break;
        } while (true);
    }
    return name;
}