#pragma once

#include <string>
#include <cctype>

inline bool jsonField(const std::string& body, const char* key, std::string& out) {
    std::string k = std::string("\"") + key + "\"";
    size_t p = 0;
    while (true) {
        p = body.find(k, p);
        if (p == std::string::npos) return false;
        size_t after = p + k.size();
        if (p > 0) {
            unsigned char prev = static_cast<unsigned char>(body[p - 1]);
            if (std::isalnum(prev) || prev == '_') {
                p = after;
                continue;
            }
        }
        while (after < body.size() && (body[after] == ' ' || body[after] == '\t')) after++;
        if (after >= body.size() || body[after] != ':') {
            p = after;
            continue;
        }
        after++;
        while (after < body.size() && (body[after] == ' ' || body[after] == '\t')) after++;
        if (after >= body.size()) return false;
        if (body[after] == '"') {
            after++;
            size_t e = body.find('"', after);
            if (e == std::string::npos) return false;
            out.assign(body, after, e - after);
            return true;
        }
        size_t e = after;
        if (body[after] == '-' || std::isdigit(static_cast<unsigned char>(body[after]))) {
            if (body[e] == '-') e++;
            while (e < body.size() && (std::isdigit(static_cast<unsigned char>(body[e])) || body[e] == '.')) e++;
            out.assign(body, after, e - after);
            return true;
        }
        p = after;
    }
}

inline bool jsonNestedValue(const std::string& body, const char* object_key, std::string& out) {
    std::string k = std::string("\"") + object_key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return false;
    p = body.find('{', p + k.size());
    if (p == std::string::npos) return false;
    size_t end = body.find('}', p);
    if (end == std::string::npos) end = body.size();
    std::string slice = body.substr(p, end - p + 1);
    return jsonField(slice, "value", out);
}

inline bool jsonFirstObjectSlice(const std::string& body, const char* array_key, std::string& slice) {
    std::string k = std::string("\"") + array_key + "\"";
    size_t p = body.find(k);
    if (p == std::string::npos) return false;
    p = body.find('[', p + k.size());
    if (p == std::string::npos) return false;
    size_t obj = body.find('{', p);
    if (obj == std::string::npos) return false;
    size_t end = body.find('}', obj);
    if (end == std::string::npos) return false;
    slice.assign(body, obj, end - obj + 1);
    return true;
}

inline int jsonContracts(const std::string& s) {
    if (s.empty()) return 0;
    try {
        float n = std::stof(s);
        if (n < 1.0f) return 0;
        return static_cast<int>(n);
    } catch (...) {
        return 0;
    }
}

// Missing size field is unknown, not empty. Live size is 1, so allow that side.
// Present 0 / <1 stays 0 and the strategy skips it.
inline int jsonSizeOrUnknown(bool found, const std::string& s) {
    if (!found) return 1;
    return jsonContracts(s);
}
