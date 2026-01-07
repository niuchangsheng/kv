#include "kv_engine.h"
#include <cassert>
#include <iostream>
#include <vector>

void test_put_get() {
    std::cout << "Testing put/get..." << std::endl;
    KVEngine kv;
    
    // Test put
    bool result = kv.put("key1", "value1");
    assert(result == true);
    
    // Test get
    std::string value;
    result = kv.get("key1", value);
    assert(result == true);
    assert(value == "value1");
    
    // Test get non-existent key
    result = kv.get("nonexistent", value);
    assert(result == false);
    
    std::cout << "Put/get test passed!" << std::endl;
}

void test_delete() {
    std::cout << "Testing delete..." << std::endl;
    KVEngine kv;
    
    // Add a key
    kv.put("key1", "value1");
    
    // Verify it exists
    std::string value;
    bool exists = kv.get("key1", value);
    assert(exists == true);
    assert(value == "value1");
    
    // Delete the key
    bool deleted = kv.Delete("key1");
    assert(deleted == true);
    
    // Verify it's gone
    exists = kv.get("key1", value);
    assert(exists == false);
    
    // Try to delete non-existent key
    deleted = kv.Delete("nonexistent");
    assert(deleted == false);
    
    std::cout << "Delete test passed!" << std::endl;
}

void test_multiple_values() {
    std::cout << "Testing multiple values..." << std::endl;
    KVEngine kv;
    
    // Add multiple key-value pairs
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string value = "value" + std::to_string(i);
        kv.put(key, value);
    }
    
    // Verify all values
    for (int i = 0; i < 100; ++i) {
        std::string key = "key" + std::to_string(i);
        std::string expected_value = "value" + std::to_string(i);
        std::string actual_value;
        
        bool found = kv.get(key, actual_value);
        assert(found);
        assert(actual_value == expected_value);
    }
    
    std::cout << "Multiple values test passed!" << std::endl;
}

int main() {
    std::cout << "Running KV Engine tests..." << std::endl;
    
    test_put_get();
    test_delete();
    test_multiple_values();
    
    std::cout << "All tests passed!" << std::endl;
    
    return 0;
}