#include "core/db.h"
#include "iterator/db_iterator.h"
#include <iostream>

DB::DB() {}

DB::~DB() {}

Status DB::Open(const Options& options, const std::string& name, DB** dbptr) {
    *dbptr = nullptr;

    // For simplicity, we'll create an in-memory database
    // In a real implementation, this would open/create files on disk
    DB* db = new DB();

    // If error_if_exists is true and we "detect" an existing database,
    // return an error. For our in-memory implementation, we'll just
    // assume the database doesn't exist yet.

    *dbptr = db;
    return Status::OK();
}

Status DB::Put(const WriteOptions& options, const std::string& key, const std::string& value) {
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
    data_store_.erase(key);
    return Status::OK();
}

Status DB::Write(const WriteOptions& options, WriteBatch* updates) {
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

Status DestroyDB(const std::string& name, const Options& options) {
    // For our in-memory implementation, there's nothing to destroy
    return Status::OK();
}
