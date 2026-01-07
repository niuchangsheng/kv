#ifndef KV_ENGINE_H
#define KV_ENGINE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

class KVEngine {
public:
    KVEngine();
    ~KVEngine();

    // Put a key-value pair
    bool put(const std::string& key, const std::string& value);

    // Get value by key
    bool get(const std::string& key, std::string& value);

    // Delete a key
    bool remove(const std::string& key);

    // Check if key exists
    bool exists(const std::string& key);

    // Get all keys
    std::vector<std::string> keys();

    // Clear all key-value pairs
    void clear();

private:
    std::unordered_map<std::string, std::string> data_store;
};

#endif // KV_ENGINE_H