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
    // Must brace-match: Polymarket US levels nest {"px":{"value":"..."}, "qty":"..."}.
    // Stopping at the first '}' truncates before qty (that was the all-sizes-1/0 bug).
    int depth = 0;
    bool in_str = false;
    for (size_t i = obj; i < body.size(); i++) {
        char c = body[i];
        if (c == '"' && (i == 0 || body[i - 1] != '\\')) in_str = !in_str;
        if (in_str) continue;
        if (c == '{') depth++;
        else if (c == '}') {
            depth--;
            if (depth == 0) {
                slice.assign(body, obj, i - obj + 1);
                return true;
            }
        }
    }
    return false;
}

inline int jsonContracts(const std::string& s) {
    if (s.empty()) return 0;
    try {
        float n = std::stof(s);
        if (n <= 0.0f) return 0;
        // US book qty can be fractional (docs: "0.50", "2.50"). Keep whole contracts.
        int whole = static_cast<int>(n);
        if (whole < 1 && n > 0.0f) return 1; // sub-1 share still means some size at that level
        return whole;
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
