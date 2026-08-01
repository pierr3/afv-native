/* http/Response.h
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

#ifndef AFV_NATIVE_HTTP_RESPONSE_H
#define AFV_NATIVE_HTTP_RESPONSE_H

#include <nlohmann/json.hpp>
#include <string>

namespace afv_native { namespace http {

    /** The outcome of one HTTP request.  Owns everything it reports, so it is
     * safe to hand to a callback on another thread.
     */
    struct Response {
        /** The transfer completed.  Says nothing about the status code - a 500
         * that arrived intact is ok == true.
         */
        bool ok = false;

        long        statusCode = 0;
        std::string contentType;
        std::string body;

        /** Transport-level error text.  Empty when ok is true. */
        std::string error;

        /** The request was aborted before it finished. */
        bool cancelled = false;

        /** Returns null for an empty body and a discarded value for one that
         * does not parse.
         */
        nlohmann::json json() const {
            if (body.empty()) {
                return nlohmann::json();
            }
            return nlohmann::json::parse(body, nullptr, false);
        }
    };

}} // namespace afv_native::http

#endif // AFV_NATIVE_HTTP_RESPONSE_H
