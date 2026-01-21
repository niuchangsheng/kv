#include "wal/wal.h"
#include <cstring>
#include <algorithm>

// CRC32 lookup table for fast checksum calculation
static const uint32_t kCRC32Table[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
    0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
    0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
    0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
    0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xab13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
    0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
    0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
    0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
    0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
    0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
    0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
    0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
    0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
    0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
    0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
    0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
    0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
    0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
    0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
    0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
    0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
    0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

// Calculate CRC32 checksum
static uint32_t CalculateCRC32(const uint8_t* data, size_t length, uint32_t crc = 0) {
    crc ^= 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc = kCRC32Table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}

// ============================================================================
// WALWriter Implementation
// ============================================================================

WALWriter::WALWriter(const std::string& log_file)
    : log_file_(log_file) {
    file_.open(log_file, std::ios::binary | std::ios::app);
    if (!file_.is_open()) {
        // Try to create the file if it doesn't exist
        file_.open(log_file, std::ios::binary | std::ios::out | std::ios::trunc);
    }
}

WALWriter::~WALWriter() {
    Close();
}

void WALWriter::WriteFixed32(uint32_t value) {
    uint8_t buf[4];
    buf[0] = static_cast<uint8_t>(value & 0xFF);
    buf[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
    buf[2] = static_cast<uint8_t>((value >> 16) & 0xFF);
    buf[3] = static_cast<uint8_t>((value >> 24) & 0xFF);
    file_.write(reinterpret_cast<const char*>(buf), 4);
}

void WALWriter::WriteFixed64(uint64_t value) {
    WriteFixed32(static_cast<uint32_t>(value & 0xFFFFFFFF));
    WriteFixed32(static_cast<uint32_t>((value >> 32) & 0xFFFFFFFF));
}

void WALWriter::WriteString(const std::string& str) {
    file_.write(str.data(), str.size());
}

uint32_t WALWriter::CalculateChecksum(RecordType type,
                                      const std::string& key,
                                      const std::string& value) const {
    // Calculate checksum over: type + key + value
    uint32_t crc = 0;
    uint8_t type_byte = static_cast<uint8_t>(type);
    crc = CalculateCRC32(&type_byte, 1, crc);
    if (!key.empty()) {
        crc = CalculateCRC32(reinterpret_cast<const uint8_t*>(key.data()), key.size(), crc);
    }
    if (!value.empty()) {
        crc = CalculateCRC32(reinterpret_cast<const uint8_t*>(value.data()), value.size(), crc);
    }
    return crc;
}

Status WALWriter::AddRecord(RecordType type,
                            const std::string& key,
                            const std::string& value) {
    if (!file_.is_open()) {
        return Status::IOError("WAL file is not open");
    }
    
    // Record format:
    // [Record Type (1 byte)]
    // [Key Length (4 bytes, little-endian)]
    // [Value Length (4 bytes, little-endian)]
    // [Key (variable)]
    // [Value (variable)]
    // [Checksum (4 bytes, little-endian)]
    
    uint32_t key_length = static_cast<uint32_t>(key.size());
    uint32_t value_length = static_cast<uint32_t>(value.size());
    
    // Write record type
    uint8_t type_byte = static_cast<uint8_t>(type);
    file_.write(reinterpret_cast<const char*>(&type_byte), 1);
    
    // Write key length
    WriteFixed32(key_length);
    
    // Write value length
    WriteFixed32(value_length);
    
    // Write key
    if (key_length > 0) {
        WriteString(key);
    }
    
    // Write value
    if (value_length > 0) {
        WriteString(value);
    }
    
    // Calculate and write checksum
    uint32_t checksum = CalculateChecksum(type, key, value);
    WriteFixed32(checksum);
    
    if (file_.fail()) {
        return Status::IOError("Failed to write to WAL file");
    }
    
    return Status::OK();
}

Status WALWriter::Sync() {
    if (!file_.is_open()) {
        return Status::IOError("WAL file is not open");
    }
    
    file_.flush();
    if (file_.fail()) {
        return Status::IOError("Failed to flush WAL file");
    }
    
    // On systems that support it, we could use fsync() here
    // For now, we'll just flush the buffer
    
    return Status::OK();
}

Status WALWriter::Close() {
    if (file_.is_open()) {
        file_.close();
        if (file_.fail()) {
            return Status::IOError("Failed to close WAL file");
        }
    }
    return Status::OK();
}

// ============================================================================
// WALReader Implementation
// ============================================================================

WALReader::WALReader(const std::string& log_file)
    : log_file_(log_file) {
    file_.open(log_file, std::ios::binary | std::ios::in);
}

WALReader::~WALReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

bool WALReader::ReadFixed32(uint32_t* value) {
    uint8_t buf[4];
    file_.read(reinterpret_cast<char*>(buf), 4);
    if (file_.gcount() != 4) {
        return false;
    }
    *value = static_cast<uint32_t>(buf[0]) |
             (static_cast<uint32_t>(buf[1]) << 8) |
             (static_cast<uint32_t>(buf[2]) << 16) |
             (static_cast<uint32_t>(buf[3]) << 24);
    return true;
}

bool WALReader::ReadFixed64(uint64_t* value) {
    uint32_t low, high;
    if (!ReadFixed32(&low) || !ReadFixed32(&high)) {
        return false;
    }
    *value = static_cast<uint64_t>(low) | (static_cast<uint64_t>(high) << 32);
    return true;
}

bool WALReader::ReadString(uint32_t length, std::string* str) {
    if (length == 0) {
        str->clear();
        return true;
    }
    
    str->resize(length);
    file_.read(&(*str)[0], length);
    return file_.gcount() == static_cast<std::streamsize>(length);
}

bool WALReader::VerifyChecksum(RecordType type,
                                const std::string& key,
                                const std::string& value,
                                uint32_t expected_checksum) const {
    uint32_t crc = 0;
    uint8_t type_byte = static_cast<uint8_t>(type);
    crc = CalculateCRC32(&type_byte, 1, crc);
    if (!key.empty()) {
        crc = CalculateCRC32(reinterpret_cast<const uint8_t*>(key.data()), key.size(), crc);
    }
    if (!value.empty()) {
        crc = CalculateCRC32(reinterpret_cast<const uint8_t*>(value.data()), value.size(), crc);
    }
    return crc == expected_checksum;
}

bool WALReader::ReadRecord(RecordType* type,
                           std::string* key,
                           std::string* value,
                           Status* status) {
    if (!file_.is_open()) {
        *status = Status::IOError("WAL file is not open");
        return false;
    }
    
    // Check for EOF
    if (file_.peek() == EOF) {
        *status = Status::OK();  // Normal EOF
        return false;
    }
    
    // Read record type
    uint8_t type_byte;
    file_.read(reinterpret_cast<char*>(&type_byte), 1);
    if (file_.gcount() != 1) {
        *status = Status::IOError("Failed to read record type");
        return false;
    }
    
    *type = static_cast<RecordType>(type_byte);
    
    // Check for EOF marker
    if (*type == kEof) {
        *status = Status::OK();
        return false;
    }
    
    // Read key length
    uint32_t key_length;
    if (!ReadFixed32(&key_length)) {
        *status = Status::IOError("Failed to read key length");
        return false;
    }
    
    // Read value length
    uint32_t value_length;
    if (!ReadFixed32(&value_length)) {
        *status = Status::IOError("Failed to read value length");
        return false;
    }
    
    // Read key
    if (!ReadString(key_length, key)) {
        *status = Status::IOError("Failed to read key");
        return false;
    }
    
    // Read value
    if (!ReadString(value_length, value)) {
        *status = Status::IOError("Failed to read value");
        return false;
    }
    
    // Read checksum
    uint32_t checksum;
    if (!ReadFixed32(&checksum)) {
        *status = Status::IOError("Failed to read checksum");
        return false;
    }
    
    // Verify checksum
    if (!VerifyChecksum(*type, *key, *value, checksum)) {
        *status = Status::Corruption("Checksum mismatch in WAL record");
        return false;
    }
    
    *status = Status::OK();
    return true;
}

Status WALReader::Replay(Handler* handler) {
    if (!file_.is_open()) {
        return Status::IOError("WAL file is not open");
    }
    
    RecordType type;
    std::string key, value;
    Status status = Status::OK();
    
    while (ReadRecord(&type, &key, &value, &status)) {
        if (!status.ok()) {
            return status;
        }
        
        Status handler_status = Status::OK();
        switch (type) {
            case kPut:
                handler_status = handler->Put(key, value);
                if (!handler_status.ok()) {
                    return handler_status;
                }
                break;
                
            case kDelete:
                handler_status = handler->Delete(key);
                if (!handler_status.ok()) {
                    return handler_status;
                }
                break;
                
            case kSync:
                // Sync point - no action needed during replay
                break;
                
            case kEof:
                // End of file - should not reach here in normal flow
                return Status::OK();
                
            default:
                return Status::Corruption("Unknown record type in WAL");
        }
    }
    
    // Check if we stopped due to an error
    if (!status.ok() && !status.IsNotFound()) {
        return status;
    }
    
    return Status::OK();
}
