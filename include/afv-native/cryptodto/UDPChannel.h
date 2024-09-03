/* cryptodto/UDPChannel.h
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

#ifndef AFV_NATIVE_UDPCHANNEL_H
#define AFV_NATIVE_UDPCHANNEL_H

#include "afv-native/Log.h"
#include "afv-native/cryptodto/Channel.h"
#include <Poco/NObserver.h>
#include <Poco/Net/DatagramSocket.h>
#include <Poco/Net/NetException.h>
#include <Poco/Net/SocketAddress.h>
#include <Poco/Net/SocketNotification.h>
#include <Poco/Net/SocketReactor.h>
#include <Poco/Thread.h>
#include <atomic>
#include <event2/event.h>
#include <functional>
#include <unordered_map>

namespace afv_native { namespace cryptodto {
    typedef void (*DtoHandlerFunc)(const std::string &dtoName, const unsigned char *bufIn, size_t bufLen, void *user_data);

    class UDPChannel: public Channel {
      private:
        std::string mAddress;

        /** mDatagramRxBuffer is the channel-internal holding buffer for a
         * freshly received datagram.  By storing it initially in a pre-reserved
         * buffer, we can avoid reallocation hell which is worse than just
         * having to copy the payload out one extra time.
         */
        unsigned char *mDatagramRxBuffer;

        Poco::Net::DatagramSocket mPocoUDPSocket;
        Poco::Net::SocketReactor  mPocoSocketReactor;
        Poco::Thread              mReactorThread;
        bool mIsOpen = false;
        std::atomic<sequence_t>   mTxSequence;
        SequenceTest              receiveSequence;

        unsigned int mAcceptableCiphers;

        void readCallback(const Poco::AutoPtr<Poco::Net::ReadableNotification> &notification);

      protected:
        std::unordered_map<std::string, std::function<void(const unsigned char *data, size_t len)>> mDtoHandlers;
        int mLastErrno;

        void enableRxMode(CryptoDtoMode mode);

        void disableRxMode(CryptoDtoMode mode);

        bool RxModeEnabled(CryptoDtoMode mode) const;

      public:
        explicit UDPChannel(int receiveSequenceHistorySize = 10);
        virtual ~UDPChannel();

        bool open();
        void close();
        bool isOpen() const;

        template <typename T>
        void sendDto(const T &pkt) {
            if (!mPocoUDPSocket.impl()) {
                LOG("UDPChannel", "tried to send on closed socket");
                return;
            }
            std::vector<unsigned char> dgBuffer(maxPermittedDatagramSize);
            sequence_t thisSeq = std::atomic_fetch_add(&mTxSequence, static_cast<sequence_t>(1));

            size_t dgSize = Encapsulate<T>(dgBuffer.data(), maxPermittedDatagramSize, thisSeq, CryptoDtoMode::CryptoModeChaCha20Poly1305, pkt);
            dgBuffer.resize(dgSize);
            if (dgSize > 0) {
                try {
                    int sent = mPocoUDPSocket.sendBytes(
                        reinterpret_cast<char *>(dgBuffer.data()), dgBuffer.size());
                    if (sent < dgBuffer.size()) {
                        LOG("udpchannel", "short write sending datagram - sent %d of %d bytes", sent,
                            dgBuffer.size());
                    }
                } catch (const Poco::Exception &ex) {
                    if (ex.code() == POCO_EWOULDBLOCK) {
                        LOG("udpchannel", "UDP packet dropped on send due to TxBuffer being full");
                    } else {
                        LOG("udpchannel", "error sending datagram: %s",
                            ex.displayText().c_str());
                    }
                }
            }
        }

        void registerDtoHandler(const std::string &dtoName, std::function<void(const unsigned char *data, size_t len)> callback);
        void unregisterDtoHandler(const std::string &dtoName);

        void setAddress(const std::string &address);

        int getLastErrno() const;

        void setChannelConfig(const dto::ChannelConfig &config) override;
    };
}} // namespace afv_native::cryptodto

#endif // AFV_NATIVE_UDPCHANNEL_H
