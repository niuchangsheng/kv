#include "core/db.h"
#include "iterator/db_iterator.h"
#include <iostream>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

DB::DB() : wal_writer_(nullptr) {}

DB::~DB() {
    if (wal_writer_) {
        wal_writer_->Close();
    }
}

Status DB::Open(const Options& options, const std::string& name, DB** dbptr) {
    *dbptr = nullptr;

    DB* db = new DB();
    db->dbname_ = name;
    
    // Create database directory if it doesn't exist
    if (options.create_if_missing) {
        if (!fs::exists(name)) {
            try {
                fs::create_directories(name);
            } catch (const std::exception& e) {
                delete db;
                return Status::IOError("Failed to create database directory: " + std::string(e.what()));
            }
        } else if (options.error_if_exists) {
            delete db;
            return Status::InvalidArgument("Database already exists");
        }
    } else {
        if (!fs::exists(name)) {
            delete db;
            return Status::NotFound("Database does not exist");
        }
    }
    
    // Open or create WAL file
    db->wal_file_ = name + "/LOG";
    db->wal_writer_ = std::make_unique<WALWriter>(db->wal_file_);
    if (!db->wal_writer_->IsOpen()) {
        delete db;
        return Status::IOError("Failed to open WAL file");
    }
    
    // Recover from WAL if it exists and has data
    Status recover_status = db->RecoverFromWAL();
    if (!recover_status.ok()) {
        delete db;
        return recover_status;
    }
    
    *dbptr = db;
    return Status::OK();
}

Status DB::Put(const WriteOptions& options, const std::string& key, const std::string& value) {
    Status status = EnsureWALOpen();
    if (!status.ok()) {
        return status;
    }
    
    // Write to WAL first
    status = wal_writer_->AddRecord(kPut, key, value);
    if (!status.ok()) {
        return status;
    }
    
    // Sync if requested
    if (options.sync) {
        status = wal_writer_->Sync();
        if (!status.ok()) {
            return status;
        }
    }
    
    // Write to in-memory store
    data_store_[key] = value;
    return Status::OK();
}

Status DB::Get(const ReadOptions& options, const std::string& key, std::string* value) {
    auto it = data_store_.find(key);
    if (it != data_store_.end()) {
        *value = it->second;
        return Status::OK();
    }
    return Status::NotFound();
}

Status DB::Delete(const WriteOptions& options, const std::string& key) {
    Status status = EnsureWALOpen();
    if (!status.ok()) {
        return status;
    }
    
    // Write to WAL first
    status = wal_writer_->AddRecord(kDelete, key, "");
    if (!status.ok()) {
        return status;
    }
    
    // Sync if requested
    if (options.sync) {
        status = wal_writer_->Sync();
        if (!status.ok()) {
            return status;
        }
    }
    
    // Delete from in-memory store
    data_store_.erase(key);
    return Status::OK();
}

Status DB::Write(const WriteOptions& options, WriteBatch* updates) {
    Status status = EnsureWALOpen();
    if (!status.ok()) {
        return status;
    }
    
    // Write all batch operations to WAL
    class BatchWALHandler : public WriteBatch::Handler {
    public:
        BatchWALHandler(WALWriter* writer) : writer_(writer), status_(Status::OK()) {}
        
        virtual void Put(const std::string& key, const std::string& value) override {
            if (status_.ok()) {
                status_ = writer_->AddRecord(kPut, key, value);
            }
        }
        
        virtual void Delete(const std::string& key) override {
            if (status_.ok()) {
                status_ = writer_->AddRecord(kDelete, key, "");
            }
        }
        
        Status GetStatus() const { return status_; }
        
    private:
        WALWriter* writer_;
        Status status_;
    };
    
    BatchWALHandler wal_handler(wal_writer_.get());
    status = updates->Iterate(&wal_handler);
    if (!status.ok()) {
        return status;
    }
    
    // Check if any WAL write failed
    status = wal_handler.GetStatus();
    if (!status.ok()) {
        return status;
    }
    
    // Sync if requested
    if (options.sync) {
        status = wal_writer_->Sync();
        if (!status.ok()) {
            return status;
        }
    }
    
    // Apply to in-memory store
    class BatchHandler : public WriteBatch::Handler {
    public:
        BatchHandler(DB* db) : db_(db) {}
        virtual void Put(const std::string& key, const std::string& value) override {
            db_->data_store_[key] = value;
        }
        virtual void Delete(const std::string& key) override {
            db_->data_store_.erase(key);
        }
    private:
        DB* db_;
    };

    BatchHandler handler(this);
    return updates->Iterate(&handler);
}

Iterator* DB::NewIterator(const ReadOptions& options) {
    return new DBIterator(data_store_);
}

Status DB::EnsureWALOpen() {
    if (!wal_writer_ || !wal_writer_->IsOpen()) {
        wal_file_ = dbname_ + "/LOG";
        wal_writer_ = std::make_unique<WALWriter>(wal_file_);
        if (!wal_writer_->IsOpen()) {
            return Status::IOError("Failed to open WAL file");
        }
    }
    return Status::OK();
}

Status DB::RecoverFromWAL() {
    // Check if WAL file exists
    if (!fs::exists(wal_file_)) {
        // No WAL file, nothing to recover
        return Status::OK();
    }
    
    // Check if WAL file is empty
    std::ifstream check_file(wal_file_, std::ios::binary | std::ios::ate);
    if (check_file.tellg() == 0) {
        // Empty WAL file, nothing to recover
        return Status::OK();
    }
    check_file.close();
    
    // Create WAL reader
    WALReader reader(wal_file_);
    if (!reader.IsOpen()) {
        return Status::IOError("Failed to open WAL file for recovery");
    }
    
    // Replay handler that applies operations to in-memory store
    class ReplayHandler : public WALReader::Handler {
    public:
        ReplayHandler(std::unordered_map<std::string, std::string>* store)
            : store_(store) {}
        
        virtual Status Put(const std::string& key, const std::string& value) override {
            (*store_)[key] = value;
            return Status::OK();
        }
        
        virtual Status Delete(const std::string& key) override {
            store_->erase(key);
            return Status::OK();
        }
        
    private:
        std::unordered_map<std::string, std::string>* store_;
    };
    
    ReplayHandler handler(&data_store_);
    return reader.Replay(&handler);
}

Status DestroyDB(const std::string& name, const Options& options) {
    try {
        if (fs::exists(name)) {
            fs::remove_all(name);
        }
        return Status::OK();
    } catch (const std::exception& e) {
        return Status::IOError("Failed to destroy database: " + std::string(e.what()));
    }
}
