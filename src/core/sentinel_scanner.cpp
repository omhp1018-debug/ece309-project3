#include "core/sentinel_scanner.h"

#include <stdexcept>
#include <utility>

SentinelScanner::SentinelScanner(std::string sentinel)
    : sentinel_(std::move(sentinel)) {
    if (sentinel_.empty()) throw std::invalid_argument("sentinel cannot be empty");
}

SentinelScanner::Out SentinelScanner::feed(std::string_view chunk) {
    Out out{"", found_};
    if (found_) return out;

    for (char c : chunk) {
        pending_.push_back(c);

        while (!pending_.empty() &&
               sentinel_.compare(0, pending_.size(), pending_) != 0) {
            out.safe_text.push_back(pending_.front());
            pending_.erase(0, 1);
        }

        if (pending_ == sentinel_) {
            pending_.clear();
            found_ = true;
            out.sentinel_found = true;
            break;
        }
    }
    return out;
}

SentinelScanner::Out SentinelScanner::flush() {
    if (found_) return {"", true};
    std::string text = std::move(pending_);
    pending_.clear();
    return {std::move(text), false};
}

