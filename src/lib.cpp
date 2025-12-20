#include "kv.h"

void KVStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lg(mu_);
    m_[key] = value;
}

std::optional<std::string> KVStore::get(const std::string& key) const {
    std::lock_guard<std::mutex> lg(mu_);
    auto it = m_.find(key);
    if (it == m_.end()) return std::nullopt;
    return it->second;
}

bool KVStore::del(const std::string& key) {
    std::lock_guard<std::mutex> lg(mu_);
    return m_.erase(key) > 0;
}

void KVStore::clear() {
    std::lock_guard<std::mutex> lg(mu_);
    m_.clear();
}
