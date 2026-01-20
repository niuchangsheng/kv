#include "options.h"

Options::Options()
    : create_if_missing(false),
      error_if_exists(false),
      paranoid_checks(false),
      info_log(nullptr) {
}

ReadOptions::ReadOptions()
    : verify_checksums(false),
      fill_cache(true),
      snapshot(nullptr) {
}

WriteOptions::WriteOptions()
    : sync(false) {
}