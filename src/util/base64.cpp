/* util/base64.cpp
 *
 * This file is part of AFV-Native.
 *
 * Copyright (c) 2019 Christopher Collins
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

#include "afv-native/util/base64.h"
#include "Poco/Base64Decoder.h"
#include "Poco/Base64Encoder.h"
#include <sstream>
#include <string>

using namespace std;
using namespace afv_native::util;

string afv_native::util::Base64Encode(const unsigned char *buffer_in, size_t len) {
    ostringstream       ostr;
    Poco::Base64Encoder b64out(ostr);
    b64out.write(reinterpret_cast<const char *>(buffer_in), len);
    b64out.close();
    return ostr.str();
}

size_t afv_native::util::Base64DecodeLen(size_t input_len) {
    return ((input_len + 3) / 4) * 3;
}

size_t afv_native::util::Base64Decode(const string &base64_in, unsigned char *buffer_out, size_t len) {
    istringstream       istr(base64_in);
    Poco::Base64Decoder b64in(istr);
    b64in.read(reinterpret_cast<char *>(buffer_out), len);
    return b64in.gcount();
}
