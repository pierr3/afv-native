#ifndef AFV_NATIVE_UTIL_OTHER_H
#define AFV_NATIVE_UTIL_OTHER_H

#include <algorithm>
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

    template <typename T>
    std::string VectorToCSV(const std::vector<T> &vec) {
        std::ostringstream out;
        if (!vec.empty()) {
            std::copy(std::begin(vec), std::end(vec) - 1, std::ostream_iterator<T>(out, ","));
            out << vec.back();
        }

        return out.str();
    }
}} // namespace afv_native::util

#endif // AFV_NATIVE_UTIL_OTHER_H
