#ifndef CRC32_H
#define CRC32_H

#include <cstdint>
#include <string>

namespace sstable {

// CRC32 checksum calculation
// Uses the same polynomial as zlib: 0xEDB88320

class CRC32 {
public:
    CRC32();
    
    // Update CRC with data
    void Update(const char* data, size_t len);
    
    // Get current CRC value
    uint32_t Get() const { return crc_ ^ 0xFFFFFFFF; }
    
    // Reset CRC
    void Reset() { crc_ = 0xFFFFFFFF; }
    
    // Calculate CRC for a string
    static uint32_t Calculate(const std::string& data) {
        CRC32 crc;
        crc.Update(data.data(), data.size());
        return crc.Get();
    }
    
    // Calculate CRC for a buffer
    static uint32_t Calculate(const char* data, size_t len) {
        CRC32 crc;
        crc.Update(data, len);
        return crc.Get();
    }

private:
    uint32_t crc_;
    static const uint32_t kTable[256];
};

}  // namespace sstable

#endif // CRC32_H
