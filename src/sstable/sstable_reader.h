#ifndef SSTABLE_READER_H
#define SSTABLE_READER_H

#include "common/status.h"
#include "block_reader.h"
#include "sstable_builder.h"
#include <string>
#include <fstream>

namespace sstable {

// SSTableReader: Reads key-value pairs from an SSTable file
class SSTableReader {
public:
    SSTableReader(const std::string& filename);
    ~SSTableReader();

    // Open the SSTable file
    Status Open();

    // Get a value by key
    Status Get(const std::string& key, std::string* value);

    // Check if the reader is valid
    bool IsValid() const { return valid_; }

private:
    // Read the footer from the end of the file
    Status ReadFooter();

    // Read the index block
    Status ReadIndexBlock();

    // Find the data block that might contain the key
    Status FindDataBlock(const std::string& key, BlockHandle* handle);

    // Read a block from the file
    Status ReadBlock(const BlockHandle& handle, std::string* block_data);

    std::string filename_;
    std::ifstream file_;
    Footer footer_;
    std::string index_block_data_;
    bool valid_;
};

}  // namespace sstable

#endif // SSTABLE_READER_H
