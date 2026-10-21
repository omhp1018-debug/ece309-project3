                                                       
  
                                                                     
                                                                    
                                                               
                          

#include "tools/workspace.h"

#include <string>

namespace fs = std::filesystem;

bool Workspace::resolve(std::string_view user_path, fs::path* out,
                        std::string* reason) const {
    if (reason == nullptr || out == nullptr) return false;
    reason->clear();
    if (user_path.empty()) {
        *reason = "empty path";
        return false;
    }
    if (user_path.find('\0') != std::string_view::npos) {
        *reason = "path contains a NUL byte";
        return false;
    }
    try {
        const fs::path p{std::string(user_path)};
        if (p.is_absolute() || p.has_root_path()) {
            *reason = "absolute paths are not allowed";
            return false;
        }
        const fs::path normalized = p.lexically_normal();
        if (normalized.begin() != normalized.end() && *normalized.begin() == fs::path("..")) {
            *reason = "path escapes workspace";
            return false;
        }
        *out = normalized == fs::path(".") ? root() : root() / normalized;
        return true;
    } catch (...) {
        *reason = "invalid path";
        return false;
    }
}
