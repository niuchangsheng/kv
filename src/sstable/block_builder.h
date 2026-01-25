#ifndef BLOCK_BUILDER_H
#define BLOCK_BUILDER_H

#include <string>
#include <vector>
#include <cstdint>

namespace sstable {

// BlockBuilder: Builds a Data Block or Index Block
// Implements shared prefix compression and restart points
class BlockBuilder {
public:
    explicit BlockBuilder(int restart_interval = 16);
    ~BlockBuilder();

    // Add a key-value pair to the block
    // Keys must be added in sorted order
    void Add(const std::string& key, const std::string& value);

    // Finish building the block
    // Returns the encoded block data
    std::string Finish();

    // Reset the builder for reuse
    void Reset();

    // Get the current size of the block (approximate)
    size_t CurrentSizeEstimate() const;

    // Check if the block is empty
    bool Empty() const { return buffer_.empty(); }

    // Get the last key added to the block
    std::string LastKey() const { return last_key_; }

private:
    // Calculate shared prefix length between two keys
    int SharedPrefixLength(const std::string& a, const std::string& b) const;

    // Add a restart point at the current position
    void AddRestartPoint();

    std::string buffer_;                    // Encoded block data
    std::vector<uint32_t> restart_points_;  // Restart point offsets
    std::string last_key_;                  // Last key added (for prefix compression)
    int restart_interval_;                  // Number of entries between restart points
    int counter_;                           // Number of entries since last restart
    bool finished_;                         // Whether Finish() has been called
};

}  // namespace sstable

#endif // BLOCK_BUILDER_H
