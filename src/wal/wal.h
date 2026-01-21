#ifndef WAL_H
#define WAL_H

#include "common/status.h"
#include <string>
#include <fstream>
#include <cstdint>
#include <climits>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <cstdio>
#endif

// Record types in WAL
enum RecordType {
    kPut = 1,      // Put operation
    kDelete = 2,   // Delete operation
    kSync = 3,     // Sync point (force flush to disk)
    kEof = 4       // End of file marker
};

// WAL Writer: writes records to WAL file
class WALWriter {
public:
    explicit WALWriter(const std::string& log_file);
    ~WALWriter();
    
    // Add a record to WAL
    // Returns OK on success, non-OK on error
    Status AddRecord(RecordType type, 
                     const std::string& key, 
                     const std::string& value);
    
    // Force flush to disk
    // Returns OK on success, non-OK on error
    Status Sync();
    
    // Close the WAL file
    // Returns OK on success, non-OK on error
    Status Close();
    
    // Check if the file is open
    bool IsOpen() const { return file_.is_open(); }
    
private:
    // Write a fixed-size value in little-endian format
    void WriteFixed32(uint32_t value);
    void WriteFixed64(uint64_t value);
    
    // Write a variable-length string
    void WriteString(const std::string& str);
    
    // Calculate CRC32 checksum
    uint32_t CalculateChecksum(RecordType type,
                                const std::string& key,
                                const std::string& value) const;
    
    std::ofstream file_;
    std::string log_file_;
    int fd_;  // File descriptor for fsync (platform-specific)
    static const size_t kBlockSize = 32768;  // 32KB block size
};

// WAL Reader: reads records from WAL file
class WALReader {
public:
    explicit WALReader(const std::string& log_file);
    ~WALReader();
    
    // Read a single record from WAL
    // Returns true if a record was read, false on EOF or error
    // On error, sets *status to non-OK
    bool ReadRecord(RecordType* type,
                    std::string* key,
                    std::string* value,
                    Status* status);
    
    // Replay all records from WAL
    // This is a callback-based approach - the handler processes each record
    class Handler {
    public:
        virtual ~Handler() {}
        virtual Status Put(const std::string& key, const std::string& value) = 0;
        virtual Status Delete(const std::string& key) = 0;
    };
    
    // Replay all records to the handler
    Status Replay(Handler* handler);
    
    // Check if the file is open
    bool IsOpen() const { return file_.is_open(); }
    
private:
    // Read a fixed-size value in little-endian format
    bool ReadFixed32(uint32_t* value);
    bool ReadFixed64(uint64_t* value);
    
    // Read a variable-length string
    bool ReadString(uint32_t length, std::string* str);
    
    // Verify CRC32 checksum
    bool VerifyChecksum(RecordType type,
                       const std::string& key,
                       const std::string& value,
                       uint32_t expected_checksum) const;
    
    std::ifstream file_;
    std::string log_file_;
};

#endif // WAL_H
