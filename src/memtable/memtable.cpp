#include "memtable/memtable.h"
#include <algorithm>

// ============================================================================
// MemTable Implementation
// ============================================================================

MemTable::MemTable() : approximate_size_(0) {}

MemTable::~MemTable() {}

void MemTable::Put(const std::string& key, const std::string& value) {
    // Check if key already exists
    auto it = table_.find(key);
    if (it != table_.end()) {
        // Update existing entry: subtract old size, add new size
        approximate_size_ -= it->first.size() + it->second.size();
        it->second = value;
        approximate_size_ += it->first.size() + it->second.size();
    } else {
        // Insert new entry
        table_[key] = value;
        approximate_size_ += key.size() + value.size();
    }
}

bool MemTable::Get(const std::string& key, std::string* value) const {
    auto it = table_.find(key);
    if (it != table_.end()) {
        // Check if this is a deletion marker (single null byte)
        if (it->second.size() == 1 && it->second[0] == '\0') {
            return false;  // Key was deleted
        }
        *value = it->second;
        return true;
    }
    return false;  // Key not found
}

void MemTable::Delete(const std::string& key) {
    auto it = table_.find(key);
    if (it != table_.end()) {
        // Update size: subtract old value size
        approximate_size_ -= it->second.size();
        // Mark as deleted with special marker (single null byte)
        // This distinguishes deletion from empty value
        it->second = std::string(1, '\0');
        approximate_size_ += 1;  // Add size of deletion marker
    } else {
        // Insert deletion marker
        table_[key] = std::string(1, '\0');
        approximate_size_ += key.size() + 1;
    }
}

size_t MemTable::ApproximateSize() const {
    return approximate_size_;
}

Iterator* MemTable::NewIterator() const {
    return new MemTableIterator(table_);
}

// ============================================================================
// MemTableIterator Implementation
// ============================================================================

MemTableIterator::MemTableIterator(const std::map<std::string, std::string>& table)
    : table_(table), iter_(table_.end()) {}

MemTableIterator::~MemTableIterator() {}

bool MemTableIterator::Valid() const {
    return iter_ != table_.end();
}

void MemTableIterator::SeekToFirst() {
    iter_ = table_.begin();
}

void MemTableIterator::SeekToLast() {
    if (table_.empty()) {
        iter_ = table_.end();
    } else {
        iter_ = --table_.end();  // Get last element
    }
}

void MemTableIterator::Seek(const std::string& target) {
    iter_ = table_.lower_bound(target);
}

void MemTableIterator::Next() {
    if (iter_ != table_.end()) {
        ++iter_;
    }
}

void MemTableIterator::Prev() {
    if (iter_ != table_.begin()) {
        --iter_;
    }
}

std::string MemTableIterator::key() const {
    if (!Valid()) {
        return "";
    }
    return iter_->first;
}

std::string MemTableIterator::value() const {
    if (!Valid()) {
        return "";
    }
    return iter_->second;
}

Status MemTableIterator::status() const {
    return Status::OK();
}
