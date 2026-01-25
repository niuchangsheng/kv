#include "sstable_reader.h"
#include "coding.h"
#include "crc32.h"
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace sstable {

SSTableReader::SSTableReader(const std::string& filename)
    : filename_(filename),
      valid_(false) {
}

SSTableReader::~SSTableReader() {
    if (file_.is_open()) {
        file_.close();
    }
}

Status SSTableReader::Open() {
    if (!fs::exists(filename_)) {
        return Status::NotFound("SSTable file not found: " + filename_);
    }

    file_.open(filename_, std::ios::binary | std::ios::in);
    if (!file_.is_open()) {
        return Status::IOError("Failed to open SSTable file: " + filename_);
    }

    // Read footer
    Status s = ReadFooter();
    if (!s.ok()) {
        return s;
    }

    // Read index block
    s = ReadIndexBlock();
    if (!s.ok()) {
        return s;
    }

    valid_ = true;
    return Status::OK();
}

Status SSTableReader::ReadFooter() {
    // Footer is always the last 48 bytes
    file_.seekg(0, std::ios::end);
    size_t file_size = file_.tellg();

    if (file_size < Footer::kEncodedLength) {
        return Status::Corruption("SSTable file too small");
    }

    file_.seekg(file_size - Footer::kEncodedLength, std::ios::beg);

    char footer_buf[Footer::kEncodedLength];
    file_.read(footer_buf, Footer::kEncodedLength);
    if (file_.gcount() != Footer::kEncodedLength) {
        return Status::IOError("Failed to read footer");
    }

    return Footer::DecodeFrom(footer_buf, &footer_);
}

Status SSTableReader::ReadIndexBlock() {
    // Read the index block
    Status s = ReadBlock(footer_.index_handle, &index_block_data_);
    if (!s.ok()) {
        return s;
    }

    return Status::OK();
}

Status SSTableReader::FindDataBlock(const std::string& key, BlockHandle* handle) {
    // Search in the index block
    BlockReader index_reader(index_block_data_);
    if (!index_reader.IsValid()) {
        return Status::Corruption("Invalid index block");
    }

    // Index entries store the last key of each data block
    // We need to find the data block that might contain the target_key
    
    // Strategy: iterate through index entries to find the right one
    // The right entry is the first one with index_key >= target_key
    // If we find index_key > target_key, we need the previous entry (if exists)
    // or the first entry if this is the first index entry
    
    Status s = index_reader.SeekToFirst();
    if (!s.ok()) {
        return s;
    }

    if (!index_reader.Valid()) {
        return Status::Corruption("Empty index block");
    }

    std::string prev_handle_encoding = "";
    std::string current_key = "";
    std::string current_handle_encoding = "";

    // Iterate through index entries
    // Index entries store the last key of each data block
    // We need to find the first index entry where index_key >= target_key
    // That data block might contain the target_key
    while (index_reader.Valid()) {
        current_key = index_reader.key();
        current_handle_encoding = index_reader.value();
        
        if (current_key >= key) {
            // Found an entry with key >= target_key
            // This data block might contain the target_key
            // (because current_key is the last key in this data block)
            if (current_handle_encoding.size() != 16) {
                return Status::Corruption("Invalid block handle in index");
            }
            return BlockHandle::DecodeFrom(current_handle_encoding.data(), handle);
        }
        
        // Save current as previous and move to next
        prev_handle_encoding = current_handle_encoding;
        s = index_reader.Next();
        if (!s.ok() && !s.IsNotFound()) {
            return s;
        }
    }

    // If we didn't find an entry >= key, key is in the last data block
    // (the one pointed to by the last index entry we saw)
    if (!current_handle_encoding.empty()) {
        if (current_handle_encoding.size() != 16) {
            return Status::Corruption("Invalid block handle in index");
        }
        return BlockHandle::DecodeFrom(current_handle_encoding.data(), handle);
    }

    // Should not reach here if index block is not empty
    return Status::Corruption("Invalid index block state");
}

Status SSTableReader::Get(const std::string& key, std::string* value) {
    if (!valid_) {
        return Status::IOError("SSTable not open");
    }

    // Find the data block
    BlockHandle data_handle;
    Status s = FindDataBlock(key, &data_handle);
    if (!s.ok()) {
        return s;
    }

    // Read the data block
    std::string block_data;
    s = ReadBlock(data_handle, &block_data);
    if (!s.ok()) {
        return s;
    }

    // Search in the data block
    BlockReader block_reader(block_data);
    if (!block_reader.IsValid()) {
        return Status::Corruption("Invalid data block");
    }

    s = block_reader.Seek(key);
    if (!s.ok()) {
        return s;
    }

    if (!block_reader.Valid()) {
        return Status::NotFound("Key not found");
    }

    // Check if the key matches
    if (block_reader.key() != key) {
        return Status::NotFound("Key not found");
    }

    // Check if it's a deletion marker (empty value with single null byte)
    std::string val = block_reader.value();
    if (val.size() == 1 && val[0] == '\0') {
        return Status::NotFound("Key deleted");
    }

    *value = val;
    return Status::OK();
}

Status SSTableReader::ReadBlock(const BlockHandle& handle, std::string* block_data) {
    // Seek to block position
    file_.seekg(static_cast<std::streamoff>(handle.offset), std::ios::beg);
    if (!file_.good()) {
        return Status::IOError("Failed to seek to block");
    }

    // Read block data
    block_data->resize(handle.size);
    file_.read(&(*block_data)[0], handle.size);
    if (file_.gcount() != static_cast<std::streamsize>(handle.size)) {
        return Status::IOError("Failed to read block data");
    }

    // Read compression type (1 byte)
    char compression_type;
    file_.read(&compression_type, 1);
    if (file_.gcount() != 1) {
        return Status::IOError("Failed to read compression type");
    }

    if (compression_type != 0) {
        return Status::NotSupported("Compression not supported");
    }

    // Read and verify CRC32 checksum (4 bytes)
    char crc_buf[4];
    file_.read(crc_buf, 4);
    if (file_.gcount() != 4) {
        return Status::IOError("Failed to read checksum");
    }

    uint32_t expected_crc = DecodeFixed32(crc_buf);
    uint32_t actual_crc = CRC32::Calculate(block_data->data(), block_data->size());

    if (expected_crc != actual_crc) {
        return Status::Corruption("Block checksum mismatch");
    }

    return Status::OK();
}

}  // namespace sstable
