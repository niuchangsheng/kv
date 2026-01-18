#ifndef WRITE_BATCH_H
#define WRITE_BATCH_H

#include "status.h"
#include <string>
#include <vector>

// WriteBatch holds a collection of updates to apply atomically to a DB.
//
// The updates are applied in the order in which they are added
// to the WriteBatch.  For example, the value of "key" will be "v3"
// after the following batch is written:
//
//    batch.Put("key", "v1");
//    batch.Delete("key");
//    batch.Put("key", "v2");
//    batch.Put("key", "v3");
class WriteBatch {
public:
    WriteBatch();
    ~WriteBatch();

    // Store the mapping "key->value" in the database.
    void Put(const std::string& key, const std::string& value);

    // If the database contains a mapping for "key", erase it.  Else do nothing.
    void Delete(const std::string& key);

    // Clear all updates buffered in this batch.
    void Clear();

    // The size of the batch (number of updates).
    size_t Count() const;

    // Support for iterating over the contents of a batch.
    class Handler {
    public:
        virtual ~Handler();
        virtual void Put(const std::string& key, const std::string& value) = 0;
        virtual void Delete(const std::string& key) = 0;
    };

    Status Iterate(Handler* handler) const;

private:
    struct Rep;
    Rep* rep_;

    // Intentionally copyable
};

#endif // WRITE_BATCH_H