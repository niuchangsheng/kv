#ifndef MEMTABLE_H
#define MEMTABLE_H

#include "common/status.h"
#include "iterator/iterator.h"
#include <string>
#include <map>
#include <memory>

// MemTable: In-memory sorted table for fast writes and reads
// Uses std::map for ordered storage (O(log n) operations)
class MemTable {
public:
    MemTable();
    ~MemTable();

    // Add or update a key-value pair
    void Put(const std::string& key, const std::string& value);

    // Get the value for a key
    // Returns true if key exists, false otherwise
    bool Get(const std::string& key, std::string* value) const;

    // Delete a key (marks as deleted with empty value)
    void Delete(const std::string& key);

    // Get approximate size in bytes (key + value lengths)
    size_t ApproximateSize() const;

    // Check if MemTable is empty
    bool Empty() const { return table_.empty(); }

    // Get number of entries
    size_t Size() const { return table_.size(); }

    // Create an iterator over the MemTable
    Iterator* NewIterator() const;

private:
    // Ordered map for key-value storage
    // Key: string key
    // Value: string value (empty string indicates deletion)
    std::map<std::string, std::string> table_;

    // Approximate size in bytes (sum of key + value lengths)
    size_t approximate_size_;

    // Maximum size before flushing (default 4MB)
    static const size_t kMaxSize = 4 * 1024 * 1024;

    // No copying allowed
    MemTable(const MemTable&);
    void operator=(const MemTable&);
};

// MemTableIterator: Iterator over MemTable entries
class MemTableIterator : public Iterator {
public:
    explicit MemTableIterator(const std::map<std::string, std::string>& table);
    ~MemTableIterator();

    // Iterator interface implementation
    bool Valid() const override;
    void SeekToFirst() override;
    void SeekToLast() override;
    void Seek(const std::string& target) override;
    void Next() override;
    void Prev() override;
    std::string key() const override;
    std::string value() const override;
    Status status() const override;

private:
    const std::map<std::string, std::string>& table_;
    std::map<std::string, std::string>::const_iterator iter_;
};

#endif // MEMTABLE_H
