#include <include/main.h>

// Source: https://nachtimwald.com/2017/11/18/base64-encode-and-decode-in-c/

size_t
b64_encoded_size(const size_t inlen)
{
    size_t ret = inlen;
    if (inlen % 3 != 0) {
        ret += 3 - (inlen % 3);
    }
    ret /= 3;
    ret *= 4;

    return ret;
}

const char b64chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

TemLangString
b64_encode(const unsigned char* in, size_t len)
{
    TemLangString str = { .allocator = currentAllocator };

    if (in == NULL || len == 0) {
        goto end;
    }

    str.used = b64_encoded_size(len);
    str.size = str.used + 1;
    str.buffer = currentAllocator->allocate(str.size);

    for (size_t i = 0, j = 0; i < len; i += 3, j += 4) {
        size_t v = in[i];
        v = i + 1 < len ? v << 8 | in[i + 1] : v << 8;
        v = i + 2 < len ? v << 8 | in[i + 2] : v << 8;

        str.buffer[j] = b64chars[(v >> 18) & 0x3F];
        str.buffer[j + 1] = b64chars[(v >> 12) & 0x3F];
        if (i + 1 < len) {
            str.buffer[j + 2] = b64chars[(v >> 6) & 0x3F];
        } else {
            str.buffer[j + 2] = '=';
        }
        if (i + 2 < len) {
            str.buffer[j + 3] = b64chars[v & 0x3F];
        } else {
            str.buffer[j + 3] = '=';
        }
    }
    TemLangStringNullTerminate(&str);

end:
    return str;
}

size_t
b64_decoded_size(const char* in)
{
    if (in == NULL) {
        return 0;
    }

    const size_t len = strlen(in);
    size_t ret = len / 4 * 3;

    for (size_t i = len; i-- > 0;) {
        if (in[i] == '=') {
            ret--;
        } else {
            break;
        }
    }

    return ret;
}

const int b64invs[] = { 62, -1, -1, -1, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60,
                        61, -1, -1, -1, -1, -1, -1, -1, 0,  1,  2,  3,  4,  5,
                        6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
                        20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1, -1, 26, 27,
                        28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
                        42, 43, 44, 45, 46, 47, 48, 49, 50, 51 };

bool
b64_isvalidchar(const char c)
{
    if (c >= '0' && c <= '9') {
        return true;
    }
    if (c >= 'A' && c <= 'Z') {
        return true;
    }
    if (c >= 'a' && c <= 'z') {
        return true;
    }
    if (c == '+' || c == '/' || c == '=') {
        return true;
    }
    return false;
}

bool
b64_decode(const char* in, pTemLangString str)
{
    if (in == NULL || str == NULL) {
        return false;
    }

    const size_t len = strlen(in);
    if (len % 4 != 0) {
        return false;
    }

    const size_t desiredLen = b64_decoded_size(in);
    if (str->size < desiredLen) {
        str->buffer = currentAllocator->reallocate(str->buffer, desiredLen + 1);
        str->size = desiredLen + 1;
    }
    str->used = desiredLen;

    for (size_t i = 0; i < len; i++) {
        if (!b64_isvalidchar(in[i])) {
            return false;
        }
    }

    for (size_t i = 0, j = 0; i < len; i += 4, j += 3) {
        size_t v = b64invs[in[i] - 43];
        v = (v << 6) | b64invs[in[i + 1] - 43];
        v = in[i + 2] == '=' ? v << 6 : (v << 6) | b64invs[in[i + 2] - 43];
        v = in[i + 3] == '=' ? v << 6 : (v << 6) | b64invs[in[i + 3] - 43];

        str->buffer[j] = (v >> 16) & 0xFF;
        if (in[i + 2] != '=') {
            str->buffer[j + 1] = (v >> 8) & 0xFF;
        }
        if (in[i + 3] != '=') {
            str->buffer[j + 2] = v & 0xFF;
        }
    }
    TemLangStringNullTerminate(str);

    return true;
}
