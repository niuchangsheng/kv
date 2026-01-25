#ifndef SSTABLE_BUILDER_H
#define SSTABLE_BUILDER_H

#include "common/status.h"
#include "block_builder.h"
#include <string>
#include <cstdint>
#include <fstream>
#include <vector>
#include <cstring>

namespace sstable {

// BlockHandle: Offset and size of a block in the SSTable file
struct BlockHandle {
    uint64_t offset;
    uint64_t size;

    BlockHandle() : offset(0), size(0) {}
    BlockHandle(uint64_t off, uint64_t sz) : offset(off), size(sz) {}

    // Encode to 16 bytes
    void EncodeTo(std::string* dst) const;

    // Decode from 16 bytes
    static Status DecodeFrom(const char* src, BlockHandle* handle);
};

// Footer: Contains Index Block and Meta Block handles
struct Footer {
    BlockHandle index_handle;
    BlockHandle meta_handle;  // Currently unused, set to 0

    static const size_t kEncodedLength = 48;  // 16 + 16 + 8 + 8
    static const uint64_t kMagicNumber = 0xdb4775248b80fb57ULL;

    // Encode footer to 48 bytes
    void EncodeTo(std::string* dst) const;

    // Decode footer from 48 bytes
    static Status DecodeFrom(const char* src, Footer* footer);
};

// SSTableBuilder: Builds an SSTable file from key-value pairs
class SSTableBuilder {
public:
    SSTableBuilder(const std::string& filename);
    ~SSTableBuilder();

    // Add a key-value pair
    // Keys must be added in sorted order
    Status Add(const std::string& key, const std::string& value);

    // Finish building the SSTable
    // Writes Index Block and Footer
    Status Finish();

    // Get the number of entries added
    uint64_t NumEntries() const { return num_entries_; }

private:
    // Flush the current data block
    Status FlushDataBlock();

    // Write a block to the file
    Status WriteBlock(const std::string& block_data, BlockHandle* handle);

    std::string filename_;
    std::ofstream file_;
    BlockBuilder data_block_builder_;
    BlockBuilder index_block_builder_;
    std::string last_key_;  // Last key in current data block (for index)
    BlockHandle current_data_block_handle_;
    std::vector<BlockHandle> data_block_handles_;
    uint64_t num_entries_;
    bool finished_;
    static const size_t kBlockSize = 4 * 1024;  // 4KB default block size
};

}  // namespace sstable

#endif // SSTABLE_BUILDER_H
