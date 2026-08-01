/* http/Request.h
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

#ifndef AFV_NATIVE_HTTP_REQUEST_H
#define AFV_NATIVE_HTTP_REQUEST_H

#include "afv-native/http/http.h"
#include <nlohmann/json.hpp>
#include <string>
#include <utility>
#include <vector>

namespace afv_native { namespace http {

    /** A request description.  Copyable, movable, owns nothing external.
     *
     * This deliberately holds no curl handle and no completion callback: a
     * Request is data handed to TransferManager::submit, which moves it into a
     * worker.  Nothing outside that worker can observe or mutate a request
     * while it is in flight, which is what removes the whole class of
     * cross-thread reset hazards the previous design had to defend against.
     */
    class Request {
      public:
        /** Every request here is a small REST call, so a transfer still
         * running after this long is wedged rather than slow.
         */
        static constexpr long kDefaultConnectTimeoutSeconds  = 10;
        static constexpr long kDefaultTransferTimeoutSeconds = 30;

        Request() = default;
        Request(std::string url, Method method);

        Request &setUrl(std::string url);

        /** Sets or replaces a header.  An empty value sends the header with no
         * value, matching curl's "Header;" form.
         */
        Request &setHeader(std::string name, std::string value);

        Request &setBody(std::string body);

        /** Serialises j and sets Content-Type: application/json.
         *
         * @throws nlohmann::json::exception if j cannot be serialised.
         */
        Request &setBody(const nlohmann::json &j);

        Request &setFollowRedirect(bool follow);

        /** Overrides the default timeouts.  A value of 0 disables that
         * timeout.
         */
        Request &setTimeouts(long connectSeconds, long transferSeconds);

        Method             method() const { return mMethod; }
        const std::string &url() const { return mUrl; }
        const std::string &body() const { return mBody; }
        bool               followRedirect() const { return mFollowRedirect; }
        long               connectTimeoutSeconds() const { return mConnectTimeoutSeconds; }
        long               transferTimeoutSeconds() const { return mTransferTimeoutSeconds; }

        const std::vector<std::pair<std::string, std::string>> &headers() const { return mHeaders; }

      private:
        Method      mMethod = Method::GET;
        std::string mUrl;
        std::string mBody;

        std::vector<std::pair<std::string, std::string>> mHeaders;

        bool mFollowRedirect         = true;
        long mConnectTimeoutSeconds  = kDefaultConnectTimeoutSeconds;
        long mTransferTimeoutSeconds = kDefaultTransferTimeoutSeconds;
    };

}} // namespace afv_native::http

#endif // AFV_NATIVE_HTTP_REQUEST_H
