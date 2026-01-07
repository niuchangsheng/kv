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

    // Delete a key
    if (kv.Delete("age")) {
        std::cout << "Deleted 'age' key" << std::endl;
    }

    std::cout << "KV Engine test completed!" << std::endl;

    return 0;
}