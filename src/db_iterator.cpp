#include "db_iterator.h"

DBIterator::DBIterator(const std::unordered_map<std::string, std::string>& data) : current_index_(0), status_(Status::OK()) {
    // Convert unordered_map to sorted vector for iteration
    sorted_data_.reserve(data.size());
    for (const auto& pair : data) {
        sorted_data_.push_back(pair);
    }
    std::sort(sorted_data_.begin(), sorted_data_.end());
}

DBIterator::~DBIterator() {}

bool DBIterator::Valid() const {
    return current_index_ < sorted_data_.size();
}

void DBIterator::Seek(const std::string& target) {
    // Find the first key >= target
    auto it = std::lower_bound(sorted_data_.begin(), sorted_data_.end(), std::make_pair(target, std::string()),
                              [](const std::pair<std::string, std::string>& a,
                                 const std::pair<std::string, std::string>& b) {
                                  return a.first < b.first;
                              });
    current_index_ = it - sorted_data_.begin();
}

void DBIterator::SeekToFirst() {
    current_index_ = 0;
}

void DBIterator::SeekToLast() {
    if (!sorted_data_.empty()) {
        current_index_ = sorted_data_.size() - 1;
    } else {
        current_index_ = 0;
    }
}

void DBIterator::Next() {
    if (Valid()) {
        ++current_index_;
    }
}

void DBIterator::Prev() {
    if (current_index_ > 0) {
        --current_index_;
    }
}

std::string DBIterator::key() const {
    if (Valid()) {
        return sorted_data_[current_index_].first;
    }
    return "";
}

std::string DBIterator::value() const {
    if (Valid()) {
        return sorted_data_[current_index_].second;
    }
    return "";
}

Status DBIterator::status() const {
    return status_;
}