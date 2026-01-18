#include "status.h"

std::string Status::ToString() const {
    if (ok()) {
        return "OK";
    }

    std::string result;
    switch (code_) {
        case kNotFound:
            result = "NotFound: ";
            break;
        case kCorruption:
            result = "Corruption: ";
            break;
        case kNotSupported:
            result = "NotSupported: ";
            break;
        case kInvalidArgument:
            result = "InvalidArgument: ";
            break;
        case kIOError:
            result = "IOError: ";
            break;
        default:
            result = "Unknown: ";
            break;
    }
    result += msg_;
    return result;
}