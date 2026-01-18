#include "kv_engine.h"
#include <iostream>

int main() {
    // Open the database
    Options options;
    options.create_if_missing = true;

    DB* db;
    Status status = DB::Open(options, "/tmp/testdb", &db);
    if (!status.ok()) {
        std::cerr << "Unable to open database: " << status.ToString() << std::endl;
        return 1;
    }

    std::cout << "Testing LevelDB-style KV Engine..." << std::endl;

    WriteOptions write_options;
    ReadOptions read_options;

    // Put some values
    status = db->Put(write_options, "name", "John");
    if (!status.ok()) {
        std::cerr << "Put failed: " << status.ToString() << std::endl;
    }

    status = db->Put(write_options, "age", "25");
    if (!status.ok()) {
        std::cerr << "Put failed: " << status.ToString() << std::endl;
    }

    status = db->Put(write_options, "city", "New York");
    if (!status.ok()) {
        std::cerr << "Put failed: " << status.ToString() << std::endl;
    }

    // Get values
    std::string value;
    status = db->Get(read_options, "name", &value);
    if (status.ok()) {
        std::cout << "name: " << value << std::endl;
    } else if (status.IsNotFound()) {
        std::cout << "name not found" << std::endl;
    } else {
        std::cerr << "Get failed: " << status.ToString() << std::endl;
    }

    status = db->Get(read_options, "age", &value);
    if (status.ok()) {
        std::cout << "age: " << value << std::endl;
    } else if (status.IsNotFound()) {
        std::cout << "age not found" << std::endl;
    } else {
        std::cerr << "Get failed: " << status.ToString() << std::endl;
    }

    // Test WriteBatch
    WriteBatch batch;
    batch.Put("batch_key1", "batch_value1");
    batch.Put("batch_key2", "batch_value2");
    batch.Delete("age");

    status = db->Write(write_options, &batch);
    if (!status.ok()) {
        std::cerr << "Write batch failed: " << status.ToString() << std::endl;
    } else {
        std::cout << "Batch write completed" << std::endl;
    }

    // Test iterator
    Iterator* it = db->NewIterator(read_options);
    std::cout << "Iterating through all key-value pairs:" << std::endl;
    for (it->SeekToFirst(); it->Valid(); it->Next()) {
        std::cout << it->key() << ": " << it->value() << std::endl;
    }

    if (!it->status().ok()) {
        std::cerr << "Iterator error: " << it->status().ToString() << std::endl;
    }

    delete it;

    // Verify age was deleted
    status = db->Get(read_options, "age", &value);
    if (status.IsNotFound()) {
        std::cout << "Confirmed: 'age' key was deleted" << std::endl;
    }

    // Clean up
    delete db;

    std::cout << "LevelDB-style KV Engine test completed!" << std::endl;

    return 0;
}