#ifndef ITERATOR_H
#define ITERATOR_H

#include "common/status.h"
#include <string>

// An iterator yields a sequence of key/value pairs from a source.
// The keys are in sorted order.
// The result of calling Next() or Prev() on an iterator initially positioned
// at the first or last key on the source is undefined.
class Iterator {
public:
    Iterator();
    virtual ~Iterator();

    // An iterator is either positioned at a key/value pair, or
    // not valid.  This method returns true iff the iterator is valid.
    virtual bool Valid() const = 0;

    // Position at the first key >= target.  The iterator may be either
    // valid or not valid after this call.  The result of Seek(target) for
    // target == "" is undefined.
    virtual void Seek(const std::string& target) = 0;

    // Position at the first key in the source.  The iterator is Valid()
    // after this call iff the source is not empty.
    virtual void SeekToFirst() = 0;

    // Position at the last key in the source.  The iterator is Valid()
    // after this call iff the source is not empty.
    virtual void SeekToLast() = 0;

    // Moves to the next entry in the source.  After this call, Valid() is
    // true iff the iterator was not positioned at the last entry in the source.
    // REQUIRES: Valid()
    virtual void Next() = 0;

    // Moves to the previous entry in the source.  After this call, Valid() is
    // true iff the iterator was not positioned at the first entry in the source.
    // REQUIRES: Valid()
    virtual void Prev() = 0;

    // Return the key for the current entry.  The underlying storage for
    // the returned string is valid only until the next modification of
    // the iterator.
    // REQUIRES: Valid()
    virtual std::string key() const = 0;

    // Return the value for the current entry.  The underlying storage for
    // the returned string is valid only until the next modification of
    // the iterator.
    // REQUIRES: Valid()
    virtual std::string value() const = 0;

    // If an error has occurred, return it.  Else return an ok status.
    // If non-ok, changes the iterator to a bad state and any future calls
    // will return non-ok.
    virtual Status status() const = 0;

private:
    // No copying allowed
    Iterator(const Iterator&);
    void operator=(const Iterator&);
};

#endif // ITERATOR_H