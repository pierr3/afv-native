/* http/CurlPerform.cpp
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

#include "afv-native/http/CurlPerform.h"
#include <array>
#include <curl/curl.h>
#include <memory>
#include <string>

using namespace afv_native::http;

namespace {
    struct EasyDeleter {
        void operator()(CURL *p) const { curl_easy_cleanup(p); }
    };

    struct SlistDeleter {
        void operator()(curl_slist *p) const { curl_slist_free_all(p); }
    };

    struct PerformState {
        std::string body;
        CancelFlag  cancel;
    };

    size_t writeCb(char *ptr, size_t size, size_t nmemb, void *userdata) {
        auto *state = static_cast<PerformState *>(userdata);
        state->body.append(ptr, size * nmemb);
        return size * nmemb;
    }

    int xferCb(void *clientp, curl_off_t, curl_off_t, curl_off_t, curl_off_t) {
        auto *state = static_cast<PerformState *>(clientp);
        // A non-zero return aborts the transfer; curl reports it as
        // CURLE_ABORTED_BY_CALLBACK.
        return (state->cancel && state->cancel->load()) ? 1 : 0;
    }
} // namespace

Response afv_native::http::curlPerform(const Request &req, const CancelFlag &cancel) {
    Response out;

    if (cancel && cancel->load()) {
        out.cancelled = true;
        out.error     = "cancelled";
        return out;
    }

    std::unique_ptr<CURL, EasyDeleter> handle(curl_easy_init());
    if (!handle) {
        out.error = "curl_easy_init failed";
        return out;
    }

    PerformState state;
    state.cancel = cancel;

    std::array<char, CURL_ERROR_SIZE> errBuf {};

    std::unique_ptr<curl_slist, SlistDeleter> headers;
    for (const auto &header: req.headers()) {
        const std::string line = header.second.empty() ? (header.first + ";")
                                                       : (header.first + ": " + header.second);
        auto *appended = curl_slist_append(headers.get(), line.c_str());
        if (appended != nullptr) {
            // curl_slist_append takes ownership of the list it was handed and
            // returns the new head; release before resetting so the old head
            // is not freed out from under it.
            (void) headers.release();
            headers.reset(appended);
        }
    }

    CURL *h = handle.get();
    curl_easy_setopt(h, CURLOPT_URL, req.url().c_str());
    curl_easy_setopt(h, CURLOPT_WRITEFUNCTION, &writeCb);
    curl_easy_setopt(h, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(h, CURLOPT_ERRORBUFFER, errBuf.data());
    curl_easy_setopt(h, CURLOPT_USERAGENT, "AFV-Native/1.0");
    curl_easy_setopt(h, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(h, CURLOPT_XFERINFOFUNCTION, &xferCb);
    curl_easy_setopt(h, CURLOPT_XFERINFODATA, &state);
    /* Required whenever libcurl is used from more than one thread. */
    curl_easy_setopt(h, CURLOPT_NOSIGNAL, 1L);
    /* Disable Nagle because Mac says so.... */
    curl_easy_setopt(h, CURLOPT_TCP_NODELAY, 1L);
    curl_easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(h, CURLOPT_CONNECTTIMEOUT, req.connectTimeoutSeconds());
    curl_easy_setopt(h, CURLOPT_TIMEOUT, req.transferTimeoutSeconds());

    if (headers) {
        curl_easy_setopt(h, CURLOPT_HTTPHEADER, headers.get());
    }

    switch (req.method()) {
        case Method::GET:
            curl_easy_setopt(h, CURLOPT_HTTPGET, 1L);
            break;
        case Method::POST:
            curl_easy_setopt(h, CURLOPT_POST, 1L);
            curl_easy_setopt(h, CURLOPT_POSTFIELDS, req.body().data());
            curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body().size()));
            break;
        case Method::PUT:
            curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "PUT");
            curl_easy_setopt(h, CURLOPT_POSTFIELDS, req.body().data());
            curl_easy_setopt(h, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.body().size()));
            break;
        case Method::DEL:
            curl_easy_setopt(h, CURLOPT_CUSTOMREQUEST, "DELETE");
            break;
    }
    curl_easy_setopt(h, CURLOPT_FOLLOWLOCATION, req.followRedirect() ? 1L : 0L);

    const CURLcode rv = curl_easy_perform(h);

    if (rv == CURLE_OK) {
        long code = 0;
        curl_easy_getinfo(h, CURLINFO_RESPONSE_CODE, &code);
        out.statusCode = code;

        char *contentType = nullptr;
        if (curl_easy_getinfo(h, CURLINFO_CONTENT_TYPE, &contentType) == CURLE_OK && contentType != nullptr) {
            out.contentType = contentType;
        }
        out.body = std::move(state.body);
        out.ok   = true;
    } else if (rv == CURLE_ABORTED_BY_CALLBACK) {
        out.cancelled = true;
        out.error     = "cancelled";
    } else {
        out.error = errBuf[0] != '\0' ? std::string(errBuf.data()) : curl_easy_strerror(rv);
    }

    return out;
}
