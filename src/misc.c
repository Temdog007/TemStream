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
    const uint64_t len = 3ULL + (random64(rs) % 7ULL);
    TemLangString name = { .allocator = currentAllocator,
                           .buffer = currentAllocator->allocate(len),
                           .size = len,
                           .used = 0 };
    TemLangStringAppendChar(&name, '@');
    for (size_t i = 0; i < len; ++i) {
        do {
            const char c = (char)(random64(rs) % 128ULL);
            if (isalnum(c)) {
                TemLangStringAppendChar(&name, c);
                break;
            }
        } while (true);
    }
    return name;
}