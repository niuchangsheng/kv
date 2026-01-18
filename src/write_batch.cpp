#include "write_batch.h"
#include "status.h"
#include <vector>
#include <string>

struct WriteBatch::Rep {
    std::vector<std::pair<std::string, std::string>> puts;
    std::vector<std::string> deletes;
};

WriteBatch::WriteBatch() : rep_(new Rep) {}

WriteBatch::~WriteBatch() {
    delete rep_;
}

void WriteBatch::Put(const std::string& key, const std::string& value) {
    rep_->puts.push_back(std::make_pair(key, value));
}

void WriteBatch::Delete(const std::string& key) {
    rep_->deletes.push_back(key);
}

void WriteBatch::Clear() {
    rep_->puts.clear();
    rep_->deletes.clear();
}

size_t WriteBatch::Count() const {
    return rep_->puts.size() + rep_->deletes.size();
}

WriteBatch::Handler::~Handler() {}

Status WriteBatch::Iterate(Handler* handler) const {
    for (const auto& put : rep_->puts) {
        handler->Put(put.first, put.second);
    }
    for (const auto& del : rep_->deletes) {
        handler->Delete(del);
    }
    return Status::OK();
}