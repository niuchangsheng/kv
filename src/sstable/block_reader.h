#ifndef BLOCK_READER_H
#define BLOCK_READER_H

#include "common/status.h"
#include <string>
#include <vector>
#include <cstdint>

namespace sstable {

// BlockReader: Reads entries from a Data Block or Index Block
// Supports seeking using restart points
class BlockReader {
public:
    BlockReader(const std::string& block_data);
    ~BlockReader();

    // Check if the block is valid
    bool IsValid() const { return valid_; }

    // Get the number of restart points
    size_t NumRestarts() const { return restart_points_.size(); }

    // Seek to the first entry
    Status SeekToFirst();

    // Seek to a specific key (using restart points for optimization)
    Status Seek(const std::string& target_key);

    // Move to the next entry
    Status Next();

    // Get the current key
    std::string key() const { return current_key_; }

    // Get the current value
    std::string value() const { return current_value_; }

    // Check if the iterator is valid
    bool Valid() const { return valid_ && !current_key_.empty(); }

private:
    // Read restart points from the end of the block
    Status ReadRestartPoints();

    // Decode an entry at the current offset
    Status DecodeEntry();

    // Rebuild the key from shared prefix
    std::string RebuildKey(uint32_t shared_len, const std::string& non_shared_key);

    // Find the restart point that contains the target key
    int FindRestartPoint(const std::string& target_key);

    std::string block_data_;
    std::vector<uint32_t> restart_points_;
    size_t data_size_;           // Size of data (excluding restart points)
    size_t current_offset_;      // Current position in the block
    std::string current_key_;
    std::string current_value_;
    std::string previous_key_;   // For key reconstruction
    bool valid_;
};

}  // namespace sstable

#endif // BLOCK_READER_H
