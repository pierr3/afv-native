#ifndef AFV_NATIVE_UTIL_OTHER_H
#define AFV_NATIVE_UTIL_OTHER_H

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace afv_native { namespace util {
    inline bool vectorContains(const std::string &key, const std::vector<std::string> &data) {
        return std::find(data.begin(), data.end(), key) != data.end();
    }

    inline auto pushbackIfUnique(const std::string key, std::vector<std::string> &data) {
        auto it = std::find(data.begin(), data.end(), key);
        if (it == data.end()) {
            data.push_back(key);
            return it;
        }
        return data.end();
    }

    inline bool removeIfExists(const std::string key, std::vector<std::string> &data) {
        auto it = std::find(data.begin(), data.end(), key);
        if (it != data.end()) {
            data.erase(it);
            return true;
        }
        return false;
    }
}} // namespace afv_native::util

#endif // AFV_NATIVE_UTIL_OTHER_H