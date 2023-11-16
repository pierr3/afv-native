#ifndef AFV_NATIVE_UTIL_OTHER_H
#define AFV_NATIVE_UTIL_OTHER_H

#include <algorithm>
#include <string>
#include <vector>

namespace afv_native { namespace util {
    inline bool vectorContains(const std::string key, const std::vector<std::string> &data) {
        return std::find(data.begin(), data.end(), key) != data.end();
    }

    inline bool pushbackIfUnique(const std::string key, std::vector<std::string> &data) {
        if (!vectorContains(key, data)) {
            data.push_back(key);
            return true;
        }

        return false;
    }

    inline bool removeIfExists(const std::string key, std::vector<std::string> &data) {
        if (vectorContains(key, data)) {
            data.erase(std::find(data.begin(), data.end(), key));
            return true;
        }
        return false;
    }
}} // namespace afv_native::util

#endif // AFV_NATIVE_UTIL_OTHER_H