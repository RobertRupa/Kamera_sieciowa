#pragma once
#include <cstddef>
inline void mbedtls_platform_zeroize(void* buffer, std::size_t length) {
    volatile unsigned char* bytes = static_cast<unsigned char*>(buffer);
    for (std::size_t index = 0; index < length; ++index) bytes[index] = 0;
}