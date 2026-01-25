#ifndef CODING_H
#define CODING_H

#include <cstdint>
#include <string>

// Encoding utilities for SSTable format
// All multi-byte values use little-endian byte order

namespace sstable {

// Write a 32-bit integer in little-endian format
inline void EncodeFixed32(char* dst, uint32_t value) {
    uint8_t* buf = reinterpret_cast<uint8_t*>(dst);
    buf[0] = static_cast<uint8_t>(value & 0xFF);
    buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
}

// Read a 32-bit integer from little-endian format
inline uint32_t DecodeFixed32(const char* src) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(src);
    return static_cast<uint32_t>(buf[0]) |
           (static_cast<uint32_t>(buf[1]) << 8) |
           (static_cast<uint32_t>(buf[2]) << 16) |
           (static_cast<uint32_t>(buf[3]) << 24);
}

// Write a 64-bit integer in little-endian format
inline void EncodeFixed64(char* dst, uint64_t value) {
    uint8_t* buf = reinterpret_cast<uint8_t*>(dst);
    buf[0] = static_cast<uint8_t>(value & 0xFF);
    buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    buf[4] = static_cast<uint8_t>((value >> 32) & 0xFF);
    buf[5] = static_cast<uint8_t>((value >> 40) & 0xFF);
    buf[6] = static_cast<uint8_t>((value >> 48) & 0xFF);
    buf[7] = static_cast<uint8_t>((value >> 56) & 0xFF);
}

// Read a 64-bit integer from little-endian format
inline uint64_t DecodeFixed64(const char* src) {
    const uint8_t* buf = reinterpret_cast<const uint8_t*>(src);
    return static_cast<uint64_t>(buf[0]) |
           (static_cast<uint64_t>(buf[1]) << 8) |
           (static_cast<uint64_t>(buf[2]) << 16) |
           (static_cast<uint64_t>(buf[3]) << 24) |
           (static_cast<uint64_t>(buf[4]) << 32) |
           (static_cast<uint64_t>(buf[5]) << 40) |
           (static_cast<uint64_t>(buf[6]) << 48) |
           (static_cast<uint64_t>(buf[7]) << 56);
}

// Encode a varint (variable-length integer)
// Returns the number of bytes written
inline int EncodeVarint32(char* dst, uint32_t value) {
    uint8_t* ptr = reinterpret_cast<uint8_t*>(dst);
    int count = 0;
    while (value >= 0x80) {
        ptr[count++] = static_cast<uint8_t>(value | 0x80);
        value >>= 7;
    }
    ptr[count++] = static_cast<uint8_t>(value);
    return count;
}

// Decode a varint from the buffer
// Returns the number of bytes read, or -1 on error
inline int DecodeVarint32(const char* src, size_t max_len, uint32_t* value) {
    const uint8_t* ptr = reinterpret_cast<const uint8_t*>(src);
    uint32_t result = 0;
    int shift = 0;
    int count = 0;
    
    for (size_t i = 0; i < max_len && i < 5; ++i) {
        uint8_t byte = ptr[i];
        result |= static_cast<uint32_t>(byte & 0x7F) << shift;
        count++;
        if ((byte & 0x80) == 0) {
            *value = result;
            return count;
        }
        shift += 7;
        if (shift >= 32) {
            return -1;  // Overflow
        }
    }
    return -1;  // Incomplete varint
}

// Calculate the length of a varint encoding
inline int VarintLength(uint32_t value) {
    int len = 1;
    while (value >= 0x80) {
        value >>= 7;
        len++;
    }
    return len;
}

}  // namespace sstable

#endif // CODING_H
