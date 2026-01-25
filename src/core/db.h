#ifndef DB_H
#define DB_H

#include "common/status.h"
#include "common/options.h"
#include "iterator/iterator.h"
#include "batch/write_batch.h"
#include "wal/wal.h"
#include "memtable/memtable.h"
#include <string>
#include <memory>
#include <mutex>

// A DB is a persistent ordered map from keys to values.
// A DB is safe for concurrent access from multiple threads without
// any external synchronization.
class DB {
public:
    DB();
    ~DB();

    // Open the database with the specified "name".
    // Stores a pointer to a heap-allocated database in *dbptr and returns
    // OK on success.
    // Stores nullptr in *dbptr and returns a non-OK status on error.
    // Caller should delete *dbptr when it is no longer needed.
    static Status Open(const Options& options, const std::string& name, DB** dbptr);

    // Set the database entry for "key" to "value".  Returns OK on success,
    // and a non-OK status on error.
    // Note: consider setting options.sync = true.
    Status Put(const WriteOptions& options, const std::string& key, const std::string& value);

    // If the database contains an entry for "key" store the
    // corresponding value in *value and return OK.
    //
    // If there is no entry for "key" leave *value unchanged and return
    // a status for which Status::IsNotFound() returns true.
    //
    // May return some other Status on an error.
    Status Get(const ReadOptions& options, const std::string& key, std::string* value);

    // Remove the database entry (if any) for "key".  Returns OK on
    // success, and a non-OK status on error.  It is not an error if "key"
    // did not exist in the database.
    // Note: consider setting options.sync = true.
    Status Delete(const WriteOptions& options, const std::string& key);

    // Apply the specified updates to the database.
    // Returns OK on success, non-OK on failure.
    // Note: consider setting options.sync = true.
    Status Write(const WriteOptions& options, WriteBatch* updates);

    // Return a heap-allocated iterator over the contents of the database.
    // The result of NewIterator() is initially invalid (caller must
    // call one of the Seek methods on the iterator before using it).
    //
    // Caller should delete the iterator when it is no longer needed.
    // The returned iterator should be deleted before this db is deleted.
    Iterator* NewIterator(const ReadOptions& options);

private:
    // MemTable for current writes
    std::unique_ptr<MemTable> memtable_;
    
    // Immutable MemTable (being flushed to SSTable)
    std::unique_ptr<MemTable> imm_memtable_;
    
    // Mutex for MemTable operations (for future thread-safety)
    std::mutex mutex_;
    
    std::string dbname_;
    std::unique_ptr<WALWriter> wal_writer_;
    std::string wal_file_;
    
    // Options
    Options options_;
    
    // Helper methods
    Status EnsureWALOpen();
    Status RecoverFromWAL();
    Status MaybeScheduleFlush();  // Check if MemTable needs flushing
    
    // No copying allowed
    DB(const DB&);
    void operator=(const DB&);
};

// Destroy the contents of the specified database.
// Be very careful using this method.
Status DestroyDB(const std::string& name, const Options& options);

#endif // DB_H
