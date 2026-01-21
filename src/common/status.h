#ifndef STATUS_H
#define STATUS_H

#include <string>

class Status {
public:
    // Create a success status.
    static Status OK() { return Status(); }

    // Create a status indicating an error.
    static Status NotFound(const std::string& msg = "Not Found") {
        return Status(kNotFound, msg);
    }

    static Status Corruption(const std::string& msg = "Corruption") {
        return Status(kCorruption, msg);
    }

    static Status NotSupported(const std::string& msg = "Not Supported") {
        return Status(kNotSupported, msg);
    }

    static Status InvalidArgument(const std::string& msg = "Invalid Argument") {
        return Status(kInvalidArgument, msg);
    }

    static Status IOError(const std::string& msg = "IO Error") {
        return Status(kIOError, msg);
    }

    // Return true if the status indicates success.
    bool ok() const { return code_ == kOk; }

    // Return true if the status indicates a NotFound error.
    bool IsNotFound() const { return code_ == kNotFound; }

    // Return true if the status indicates a Corruption error.
    bool IsCorruption() const { return code_ == kCorruption; }

    // Return true if the status indicates an IOError.
    bool IsIOError() const { return code_ == kIOError; }

    // Return true if the status indicates an InvalidArgument error.
    bool IsInvalidArgument() const { return code_ == kInvalidArgument; }

    // Return a string representation of this status.
    std::string ToString() const;

private:
    enum Code {
        kOk = 0,
        kNotFound = 1,
        kCorruption = 2,
        kNotSupported = 3,
        kInvalidArgument = 4,
        kIOError = 5
    };

    Status() : code_(kOk) {}
    Status(Code code, const std::string& msg) : code_(code), msg_(msg) {}

    Code code_;
    std::string msg_;
};

#endif // STATUS_H