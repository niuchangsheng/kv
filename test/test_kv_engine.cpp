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

void test_remove() {
    std::cout << "Testing remove..." << std::endl;
    KVEngine kv;
    
    // Add a key
    kv.put("key1", "value1");
    
    // Verify it exists
    std::string value;
    bool exists = kv.get("key1", value);
    assert(exists == true);
    assert(value == "value1");
    
    // Remove the key
    bool removed = kv.remove("key1");
    assert(removed == true);
    
    // Verify it's gone
    exists = kv.get("key1", value);
    assert(exists == false);
    
    // Try to remove non-existent key
    removed = kv.remove("nonexistent");
    assert(removed == false);
    
    std::cout << "Remove test passed!" << std::endl;
}

void test_exists() {
    std::cout << "Testing exists..." << std::endl;
    KVEngine kv;
    
    // Test non-existent key
    bool result = kv.exists("nonexistent");
    assert(result == false);
    
    // Add a key
    kv.put("key1", "value1");
    
    // Test existing key
    result = kv.exists("key1");
    assert(result == true);
    
    // Remove the key
    kv.remove("key1");
    
    // Test that it no longer exists
    result = kv.exists("key1");
    assert(result == false);
    
    std::cout << "Exists test passed!" << std::endl;
}

void test_keys() {
    std::cout << "Testing keys..." << std::endl;
    KVEngine kv;
    
    // Test empty store
    std::vector<std::string> keys = kv.keys();
    assert(keys.size() == 0);
    
    // Add some keys
    kv.put("key1", "value1");
    kv.put("key2", "value2");
    kv.put("key3", "value3");
    
    // Get all keys
    keys = kv.keys();
    assert(keys.size() == 3);
    
    // Verify all keys are present (order doesn't matter for this test)
    bool found_key1 = false, found_key2 = false, found_key3 = false;
    for (const auto& key : keys) {
        if (key == "key1") found_key1 = true;
        if (key == "key2") found_key2 = true;
        if (key == "key3") found_key3 = true;
    }
    
    assert(found_key1);
    assert(found_key2);
    assert(found_key3);
    
    std::cout << "Keys test passed!" << std::endl;
}

void test_clear() {
    std::cout << "Testing clear..." << std::endl;
    KVEngine kv;
    
    // Add some keys
    kv.put("key1", "value1");
    kv.put("key2", "value2");
    
    // Verify they exist
    assert(kv.exists("key1"));
    assert(kv.exists("key2"));
    
    // Clear the store
    kv.clear();
    
    // Verify they're gone
    assert(!kv.exists("key1"));
    assert(!kv.exists("key2"));
    
    // Verify keys list is empty
    std::vector<std::string> keys = kv.keys();
    assert(keys.size() == 0);
    
    std::cout << "Clear test passed!" << std::endl;
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
    test_remove();
    test_exists();
    test_keys();
    test_clear();
    test_multiple_values();
    
    std::cout << "All tests passed!" << std::endl;
    
    return 0;
}