#include "block_builder.h"
#include "coding.h"
#include <algorithm>
#include <cstring>

namespace sstable {

BlockBuilder::BlockBuilder(int restart_interval)
    : restart_interval_(restart_interval),
      counter_(0),
      finished_(false) {
    restart_points_.push_back(0);  // First restart point at offset 0
}

BlockBuilder::~BlockBuilder() {}

void BlockBuilder::Add(const std::string& key, const std::string& value) {
    if (finished_) {
        return;  // Cannot add after Finish()
    }

    // Check if we need a restart point
    if (counter_ >= restart_interval_) {
        AddRestartPoint();
        counter_ = 0;
    }

    // Calculate shared prefix length
    int shared_len = 0;
    if (!last_key_.empty()) {
        shared_len = SharedPrefixLength(last_key_, key);
    }

    // Encode the entry
    int non_shared_key_len = key.size() - shared_len;
    int value_len = value.size();

    // Write shared_len (varint)
    char varint_buf[5];
    int varint_len = EncodeVarint32(varint_buf, shared_len);
    buffer_.append(varint_buf, varint_len);

    // Write non_shared_key_len (varint)
    varint_len = EncodeVarint32(varint_buf, non_shared_key_len);
    buffer_.append(varint_buf, varint_len);

    // Write value_len (varint)
    varint_len = EncodeVarint32(varint_buf, value_len);
    buffer_.append(varint_buf, varint_len);

    // Write non-shared key part
    if (non_shared_key_len > 0) {
        buffer_.append(key.data() + shared_len, non_shared_key_len);
    }

    // Write value
    buffer_.append(value.data(), value_len);

    // Update state
    last_key_ = key;
    counter_++;
}

void BlockBuilder::AddRestartPoint() {
    restart_points_.push_back(static_cast<uint32_t>(buffer_.size()));
}

int BlockBuilder::SharedPrefixLength(const std::string& a, const std::string& b) const {
    size_t min_len = std::min(a.size(), b.size());
    for (size_t i = 0; i < min_len; ++i) {
        if (a[i] != b[i]) {
            return static_cast<int>(i);
        }
    }
    return static_cast<int>(min_len);
}

std::string BlockBuilder::Finish() {
    if (finished_) {
        return buffer_;
    }

    // Add restart points array
    size_t restart_offset = buffer_.size();
    for (uint32_t offset : restart_points_) {
        char buf[4];
        EncodeFixed32(buf, offset);
        buffer_.append(buf, 4);
    }

    // Add restart point count
    char buf[4];
    EncodeFixed32(buf, static_cast<uint32_t>(restart_points_.size()));
    buffer_.append(buf, 4);

    finished_ = true;
    return buffer_;
}

void BlockBuilder::Reset() {
    buffer_.clear();
    restart_points_.clear();
    restart_points_.push_back(0);
    last_key_.clear();
    counter_ = 0;
    finished_ = false;
}

size_t BlockBuilder::CurrentSizeEstimate() const {
    // Estimate: current buffer size + restart points overhead
    return buffer_.size() + restart_points_.size() * 4 + 4;  // 4 bytes per offset + 4 bytes for count
}

}  // namespace sstable
