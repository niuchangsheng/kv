#ifndef DB_ITERATOR_H
#define DB_ITERATOR_H

#include "iterator.h"
#include <unordered_map>
#include <string>
#include <vector>
#include <algorithm>

// A simple iterator implementation for our in-memory database
class DBIterator : public Iterator {
public:
    DBIterator(const std::unordered_map<std::string, std::string>& data);
    ~DBIterator();

    virtual bool Valid() const override;
    virtual void Seek(const std::string& target) override;
    virtual void SeekToFirst() override;
    virtual void SeekToLast() override;
    virtual void Next() override;
    virtual void Prev() override;
    virtual std::string key() const override;
    virtual std::string value() const override;
    virtual Status status() const override;

private:
    std::vector<std::pair<std::string, std::string>> sorted_data_;
    size_t current_index_;
    Status status_;
};

#endif // DB_ITERATOR_H