#include "block_reader.h"
#include "coding.h"
#include <algorithm>

namespace sstable {

BlockReader::BlockReader(const std::string& block_data)
    : block_data_(block_data),
      current_offset_(0),
      valid_(false) {
    if (block_data_.size() < 4) {
        valid_ = false;
        return;
    }
    ReadRestartPoints();
}

BlockReader::~BlockReader() {}

Status BlockReader::ReadRestartPoints() {
    if (block_data_.size() < 4) {
        valid_ = false;
        return Status::Corruption("Block too small");
    }

    // Read restart point count (last 4 bytes)
    size_t count_offset = block_data_.size() - 4;
    uint32_t num_restarts = DecodeFixed32(block_data_.data() + count_offset);

    if (num_restarts == 0 || block_data_.size() < 4 + num_restarts * 4) {
        valid_ = false;
        return Status::Corruption("Invalid restart point count");
    }

    // Read restart points
    restart_points_.clear();
    size_t restart_offset = block_data_.size() - 4 - num_restarts * 4;
    data_size_ = restart_offset;

    for (uint32_t i = 0; i < num_restarts; ++i) {
        uint32_t offset = DecodeFixed32(block_data_.data() + restart_offset + i * 4);
        if (offset > data_size_) {
            valid_ = false;
            return Status::Corruption("Invalid restart point offset");
        }
        restart_points_.push_back(offset);
    }

    valid_ = true;
    return Status::OK();
}

Status BlockReader::SeekToFirst() {
    if (!valid_ || restart_points_.empty()) {
        return Status::Corruption("Invalid block");
    }

    current_offset_ = restart_points_[0];
    previous_key_.clear();
    return DecodeEntry();
}

Status BlockReader::Seek(const std::string& target_key) {
    if (!valid_ || restart_points_.empty()) {
        return Status::Corruption("Invalid block");
    }

    // Binary search in restart points
    int restart_idx = FindRestartPoint(target_key);
    if (restart_idx < 0) {
        restart_idx = 0;
    }

    // Start from the restart point
    current_offset_ = restart_points_[restart_idx];
    previous_key_.clear();

    // Decode entries until we find the target or pass it
    Status status = DecodeEntry();
    while (status.ok() && Valid()) {
        if (current_key_ >= target_key) {
            return Status::OK();
        }
        status = Next();
    }

    return status;
}

int BlockReader::FindRestartPoint(const std::string& target_key) {
    // Binary search for the restart point that might contain target_key
    int left = 0;
    int right = static_cast<int>(restart_points_.size()) - 1;
    int result = -1;

    while (left <= right) {
        int mid = (left + right) / 2;
        
        // Decode the key at this restart point
        size_t save_offset = current_offset_;
        std::string save_prev_key = previous_key_;
        
        current_offset_ = restart_points_[mid];
        previous_key_.clear();
        
        if (DecodeEntry().ok() && Valid()) {
            if (current_key_ >= target_key) {
                result = mid;
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        } else {
            break;
        }
        
        current_offset_ = save_offset;
        previous_key_ = save_prev_key;
    }

    return result >= 0 ? result : static_cast<int>(restart_points_.size()) - 1;
}

Status BlockReader::Next() {
    if (current_offset_ >= data_size_) {
        current_key_.clear();
        current_value_.clear();
        return Status::NotFound("End of block");
    }

    return DecodeEntry();
}

Status BlockReader::DecodeEntry() {
    if (current_offset_ >= data_size_) {
        current_key_.clear();
        current_value_.clear();
        return Status::NotFound("End of block");
    }

    const char* data = block_data_.data() + current_offset_;
    size_t remaining = data_size_ - current_offset_;

    // Decode shared_len
    uint32_t shared_len;
    int varint_len = DecodeVarint32(data, remaining, &shared_len);
    if (varint_len < 0) {
        current_key_.clear();
        current_value_.clear();
        return Status::Corruption("Failed to decode shared_len");
    }
    data += varint_len;
    remaining -= varint_len;

    // Decode non_shared_key_len
    uint32_t non_shared_key_len;
    varint_len = DecodeVarint32(data, remaining, &non_shared_key_len);
    if (varint_len < 0) {
        current_key_.clear();
        current_value_.clear();
        return Status::Corruption("Failed to decode non_shared_key_len");
    }
    data += varint_len;
    remaining -= varint_len;

    // Decode value_len
    uint32_t value_len;
    varint_len = DecodeVarint32(data, remaining, &value_len);
    if (varint_len < 0) {
        current_key_.clear();
        current_value_.clear();
        return Status::Corruption("Failed to decode value_len");
    }
    data += varint_len;
    remaining -= varint_len;

    // Check bounds
    if (non_shared_key_len > remaining || value_len > remaining - non_shared_key_len) {
        current_key_.clear();
        current_value_.clear();
        return Status::Corruption("Entry exceeds block bounds");
    }

    // Read non-shared key part
    std::string non_shared_key(data, non_shared_key_len);
    data += non_shared_key_len;
    remaining -= non_shared_key_len;

    // Read value
    current_value_.assign(data, value_len);
    data += value_len;

    // Rebuild the full key
    current_key_ = RebuildKey(shared_len, non_shared_key);
    previous_key_ = current_key_;

    // Update offset
    current_offset_ = data - block_data_.data();

    return Status::OK();
}

std::string BlockReader::RebuildKey(uint32_t shared_len, const std::string& non_shared_key) {
    if (shared_len == 0) {
        // Restart point: full key stored
        return non_shared_key;
    } else {
        // Use shared prefix from previous key
        if (previous_key_.size() < shared_len) {
            return "";  // Error: invalid shared_len
        }
        return previous_key_.substr(0, shared_len) + non_shared_key;
    }
}

}  // namespace sstable
