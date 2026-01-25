#include "sstable_builder.h"
#include "coding.h"
#include "crc32.h"
#include <fstream>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace sstable {

void BlockHandle::EncodeTo(std::string* dst) const {
    char buf[16];
    EncodeFixed64(buf, offset);
    EncodeFixed64(buf + 8, size);
    dst->append(buf, 16);
}

Status BlockHandle::DecodeFrom(const char* src, BlockHandle* handle) {
    handle->offset = DecodeFixed64(src);
    handle->size = DecodeFixed64(src + 8);
    return Status::OK();
}

void Footer::EncodeTo(std::string* dst) const {
    char buf[kEncodedLength];
    
    // Index Block Handle (16 bytes)
    index_handle.EncodeTo(dst);
    
    // Meta Block Handle (16 bytes) - currently unused
    meta_handle.EncodeTo(dst);
    
    // Padding (8 bytes)
    memset(buf, 0, 8);
    dst->append(buf, 8);
    
    // Magic Number (8 bytes)
    EncodeFixed64(buf, kMagicNumber);
    dst->append(buf, 8);
}

Status Footer::DecodeFrom(const char* src, Footer* footer) {
    // Decode Index Block Handle
    Status s = BlockHandle::DecodeFrom(src, &footer->index_handle);
    if (!s.ok()) return s;
    
    // Decode Meta Block Handle
    s = BlockHandle::DecodeFrom(src + 16, &footer->meta_handle);
    if (!s.ok()) return s;
    
    // Verify Magic Number (at offset 40, after 16+16+8 bytes)
    uint64_t magic = DecodeFixed64(src + 40);
    if (magic != kMagicNumber) {
        return Status::Corruption("Invalid SSTable magic number");
    }
    
    return Status::OK();
}

SSTableBuilder::SSTableBuilder(const std::string& filename)
    : filename_(filename),
      data_block_builder_(16),  // restart_interval = 16
      index_block_builder_(1),  // restart_interval = 1 (no compression for index)
      num_entries_(0),
      finished_(false) {
    // Create parent directory if needed
    fs::path file_path(filename);
    if (file_path.has_parent_path()) {
        fs::create_directories(file_path.parent_path());
    }
    
    file_.open(filename, std::ios::binary | std::ios::out);
}

SSTableBuilder::~SSTableBuilder() {
    if (file_.is_open()) {
        file_.close();
    }
}

Status SSTableBuilder::Add(const std::string& key, const std::string& value) {
    if (finished_) {
        return Status::InvalidArgument("Cannot add after Finish()");
    }

    if (!file_.is_open()) {
        return Status::IOError("File not open");
    }

    // Check if we need to flush the current data block (before adding)
    if (data_block_builder_.CurrentSizeEstimate() >= kBlockSize && !data_block_builder_.Empty()) {
        Status s = FlushDataBlock();
        if (!s.ok()) {
            return s;
        }
    }

    // Add to data block
    data_block_builder_.Add(key, value);
    last_key_ = key;
    num_entries_++;

    return Status::OK();
}

Status SSTableBuilder::FlushDataBlock() {
    if (data_block_builder_.Empty()) {
        return Status::OK();
    }

    // Get the last key from the data block builder (before Finish())
    std::string last_key = data_block_builder_.LastKey();
    
    // Finish the data block
    std::string block_data = data_block_builder_.Finish();
    
    // Write the block
    BlockHandle handle;
    Status s = WriteBlock(block_data, &handle);
    if (!s.ok()) {
        return s;
    }

    // Add index entry: last key -> block handle
    std::string handle_encoding;
    handle.EncodeTo(&handle_encoding);
    index_block_builder_.Add(last_key, handle_encoding);

    // Reset data block builder
    data_block_builder_.Reset();
    last_key_.clear();

    return Status::OK();
}

Status SSTableBuilder::WriteBlock(const std::string& block_data, BlockHandle* handle) {
    if (!file_.is_open()) {
        return Status::IOError("File not open");
    }

    // Get current file position (offset)
    handle->offset = static_cast<uint64_t>(file_.tellp());
    handle->size = block_data.size();

    // Write block data
    file_.write(block_data.data(), block_data.size());
    if (!file_.good()) {
        return Status::IOError("Failed to write block");
    }

    // Write compression type (1 byte: 0 = no compression)
    char compression_type = 0;
    file_.write(&compression_type, 1);
    if (!file_.good()) {
        return Status::IOError("Failed to write compression type");
    }

    // Calculate and write CRC32 checksum (4 bytes)
    uint32_t crc = CRC32::Calculate(block_data.data(), block_data.size());
    char crc_buf[4];
    EncodeFixed32(crc_buf, crc);
    file_.write(crc_buf, 4);
    if (!file_.good()) {
        return Status::IOError("Failed to write checksum");
    }

    return Status::OK();
}

Status SSTableBuilder::Finish() {
    if (finished_) {
        return Status::OK();
    }

    if (!file_.is_open()) {
        return Status::IOError("File not open");
    }

    // Flush any remaining data block
    Status s = FlushDataBlock();
    if (!s.ok()) {
        return s;
    }

    // Finish and write Index Block
    std::string index_block_data = index_block_builder_.Finish();
    BlockHandle index_handle;
    s = WriteBlock(index_block_data, &index_handle);
    if (!s.ok()) {
        return s;
    }

    // Write Footer
    Footer footer;
    footer.index_handle = index_handle;
    footer.meta_handle = BlockHandle(0, 0);  // No meta block for now

    std::string footer_encoding;
    footer.EncodeTo(&footer_encoding);
    file_.write(footer_encoding.data(), footer_encoding.size());
    if (!file_.good()) {
        return Status::IOError("Failed to write footer");
    }

    file_.close();
    finished_ = true;

    return Status::OK();
}

}  // namespace sstable
