#include "core/db.h"
#include "iterator/db_iterator.h"
#include "memtable/memtable.h"
#include "sstable/sstable_builder.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

DB::DB() : wal_writer_(nullptr), next_file_number_(1) {
    memtable_ = std::make_unique<MemTable>();
}

DB::~DB() {
    if (wal_writer_) {
        wal_writer_->Close();
    }
}

Status DB::Open(const Options& options, const std::string& name, DB** dbptr) {
    *dbptr = nullptr;

    DB* db = new DB();
    db->dbname_ = name;
    db->options_ = options;
    
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
    
    // Check if recovered MemTable needs flushing
    if (db->memtable_->ApproximateSize() > db->options_.write_buffer_size) {
        // TODO: Schedule flush to SSTable (Phase 3)
        // For now, we just log that it needs flushing
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
    
    // Write to MemTable
    memtable_->Put(key, value);
    
    // Check if MemTable needs flushing
    Status flush_status = MaybeScheduleFlush();
    if (!flush_status.ok()) {
        return flush_status;
    }
    
    return Status::OK();
}

Status DB::Get(const ReadOptions& options, const std::string& key, std::string* value) {
    // First check current MemTable
    if (memtable_->Get(key, value)) {
        return Status::OK();
    }
    
    // Then check Immutable MemTable (if exists)
    if (imm_memtable_ && imm_memtable_->Get(key, value)) {
        return Status::OK();
    }
    
    // Check SSTables (search in reverse order - newer files first)
    return GetFromSSTable(key, value);
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
    
    // Delete from MemTable
    memtable_->Delete(key);
    
    // Check if MemTable needs flushing
    Status flush_status = MaybeScheduleFlush();
    if (!flush_status.ok()) {
        return flush_status;
    }
    
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
    
    // Apply to MemTable
    class BatchHandler : public WriteBatch::Handler {
    public:
        BatchHandler(MemTable* memtable) : memtable_(memtable) {}
        virtual void Put(const std::string& key, const std::string& value) override {
            memtable_->Put(key, value);
        }
        virtual void Delete(const std::string& key) override {
            memtable_->Delete(key);
        }
    private:
        MemTable* memtable_;
    };

    BatchHandler handler(memtable_.get());
    Status batch_status = updates->Iterate(&handler);
    if (!batch_status.ok()) {
        return batch_status;
    }
    
    // Check if MemTable needs flushing
    Status flush_status = MaybeScheduleFlush();
    if (!flush_status.ok()) {
        return flush_status;
    }
    
    return Status::OK();
}

Iterator* DB::NewIterator(const ReadOptions& options) {
    // TODO: Merge iterator from MemTable, Immutable MemTable, and SSTables (Phase 3)
    // For now, return MemTable iterator
    return memtable_->NewIterator();
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
    
    // Replay handler that applies operations to MemTable
    class ReplayHandler : public WALReader::Handler {
    public:
        ReplayHandler(MemTable* memtable)
            : memtable_(memtable) {}
        
        virtual Status Put(const std::string& key, const std::string& value) override {
            memtable_->Put(key, value);
            return Status::OK();
        }
        
        virtual Status Delete(const std::string& key) override {
            memtable_->Delete(key);
            return Status::OK();
        }
        
    private:
        MemTable* memtable_;
    };
    
    ReplayHandler handler(memtable_.get());
    return reader.Replay(&handler);
}

Status DB::MaybeScheduleFlush() {
    // Check if MemTable size exceeds threshold
    // Use write_buffer_size if set, otherwise use default 4MB (same as MemTable::kMaxSize)
    size_t threshold = options_.write_buffer_size > 0 
        ? options_.write_buffer_size 
        : 4 * 1024 * 1024;  // Default 4MB
    
    if (memtable_->ApproximateSize() > threshold) {
        // Make current MemTable immutable
        if (!imm_memtable_) {
            imm_memtable_ = std::move(memtable_);
            memtable_ = std::make_unique<MemTable>();
            
            // Flush Immutable MemTable to SSTable
            Status flush_status = FlushMemTable();
            if (!flush_status.ok()) {
                return flush_status;
            }
        }
        // If imm_memtable_ already exists, we need to wait for flush to complete
        // This is a simplified implementation - in production, we'd block or return error
    }
    return Status::OK();
}

Status DB::FlushMemTable() {
    if (!imm_memtable_ || imm_memtable_->Empty()) {
        return Status::OK();
    }

    // Generate SSTable filename
    std::string sstable_file = dbname_ + "/0/" + std::to_string(next_file_number_++) + ".sst";
    
    // Create Level 0 directory if needed
    fs::path level0_dir = fs::path(sstable_file).parent_path();
    if (!fs::exists(level0_dir)) {
        fs::create_directories(level0_dir);
    }

    // Build SSTable from Immutable MemTable
    sstable::SSTableBuilder builder(sstable_file);
    
    // Iterate through MemTable and add entries
    Iterator* it = imm_memtable_->NewIterator();
    it->SeekToFirst();
    
    while (it->Valid()) {
        std::string key = it->key();
        std::string value = it->value();
        
        // Skip deletion markers (empty value with single null byte)
        // But still write them to SSTable for proper deletion propagation
        Status s = builder.Add(key, value);
        if (!s.ok()) {
            delete it;
            return s;
        }
        
        it->Next();
    }
    delete it;

    // Finish building the SSTable
    Status s = builder.Finish();
    if (!s.ok()) {
        return s;
    }

    // Add to SSTable file list
    sstable_files_.push_back(sstable_file);

    // Clear Immutable MemTable
    imm_memtable_.reset();

    return Status::OK();
}

Status DB::GetFromSSTable(const std::string& key, std::string* value) {
    // Search SSTables in reverse order (newer files first)
    for (auto it = sstable_files_.rbegin(); it != sstable_files_.rend(); ++it) {
        sstable::SSTableReader reader(*it);
        Status s = reader.Open();
        if (!s.ok()) {
            continue;  // Skip corrupted files
        }

        s = reader.Get(key, value);
        if (s.ok()) {
            return Status::OK();
        } else if (s.IsNotFound()) {
            // Key might be deleted in this file, continue searching
            continue;
        } else {
            // Error reading file, continue to next
            continue;
        }
    }

    return Status::NotFound();
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
