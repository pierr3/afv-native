/* http/Request.cpp
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2015,2019-2020 Christopher Collins
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "afv-native/http/Request.h"
#include <utility>

using namespace afv_native::http;

Request::Request(std::string url, Method method):
    mMethod(method), mUrl(std::move(url)), mFollowRedirect(method == Method::GET) {
}

Request &Request::setUrl(std::string url) {
    mUrl = std::move(url);
    return *this;
}

Request &Request::setHeader(std::string name, std::string value) {
    for (auto &header: mHeaders) {
        if (header.first == name) {
            header.second = std::move(value);
            return *this;
        }
    }
    mHeaders.emplace_back(std::move(name), std::move(value));
    return *this;
}

Request &Request::setBody(std::string body) {
    mBody = std::move(body);
    return *this;
}

Request &Request::setBody(const nlohmann::json &j) {
    mBody = j.dump();
    return setHeader("Content-Type", "application/json; charset=UTF-8");
}

Request &Request::setFollowRedirect(bool follow) {
    mFollowRedirect = follow;
    return *this;
}

Request &Request::setTimeouts(long connectSeconds, long transferSeconds) {
    mConnectTimeoutSeconds  = connectSeconds;
    mTransferTimeoutSeconds = transferSeconds;
    return *this;
}
