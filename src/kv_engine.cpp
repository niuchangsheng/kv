#include "kv_engine.h"

KVEngine::KVEngine() {
    // Constructor implementation
}

KVEngine::~KVEngine() {
    // Destructor implementation
}

bool KVEngine::put(const std::string& key, const std::string& value) {
    data_store[key] = value;
    return true;
}

bool KVEngine::get(const std::string& key, std::string& value) {
    auto it = data_store.find(key);
    if (it != data_store.end()) {
        value = it->second;
        return true;
    }
    return false;
}

bool KVEngine::remove(const std::string& key) {
    if (data_store.find(key) != data_store.end()) {
        data_store.erase(key);
        return true;
    }
    return false;
}

bool KVEngine::exists(const std::string& key) {
    return data_store.find(key) != data_store.end();
}

std::vector<std::string> KVEngine::keys() {
    std::vector<std::string> result;
    for (const auto& pair : data_store) {
        result.push_back(pair.first);
    }
    return result;
}

void KVEngine::clear() {
    data_store.clear();
}