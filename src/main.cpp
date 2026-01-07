#include "kv_engine.h"
#include <iostream>

int main() {
    KVEngine kv;

    // Test basic operations
    std::cout << "Testing KV Engine..." << std::endl;

    // Put some values
    kv.put("name", "John");
    kv.put("age", "25");
    kv.put("city", "New York");

    // Get values
    std::string value;
    if (kv.get("name", value)) {
        std::cout << "name: " << value << std::endl;
    }

    if (kv.get("age", value)) {
        std::cout << "age: " << value << std::endl;
    }

    // Check if key exists
    if (kv.exists("city")) {
        std::cout << "city key exists" << std::endl;
    }

    // Get all keys
    auto all_keys = kv.keys();
    std::cout << "All keys: ";
    for (const auto& key : all_keys) {
        std::cout << key << " ";
    }
    std::cout << std::endl;

    // Remove a key
    if (kv.remove("age")) {
        std::cout << "Removed 'age' key" << std::endl;
    }

    std::cout << "KV Engine test completed!" << std::endl;

    return 0;
}