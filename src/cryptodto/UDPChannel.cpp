/* cryptodto/UDPChannel.cpp
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

#include "afv-native/cryptodto/UDPChannel.h"
#include "afv-native/cryptodto/dto/ChannelConfig.h"
#include <Poco/Net/IPAddress.h>
#include <cerrno>
#include <event2/util.h>

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #include <afv-native/cryptodto/dto/ChannelConfig.h>
    #include <winsock.h>
    #include <ws2ipdef.h>

#else
    #include <netinet/in.h>
    #include <netinet/ip.h>
    #include <netinet/ip6.h>
    #include <sys/socket.h>
    #include <unistd.h>
#endif

#include "afv-native/Log.h"

using namespace afv_native::cryptodto;
using namespace std;

UDPChannel::UDPChannel(struct event_base *evBase, int receiveSequenceHistorySize):
    Channel(), mAddress(), mDatagramRxBuffer(nullptr), mPocoUDPSocket(), mPocoSocketReactor(), mReactorThread("UDP Socket Reactor Thread"), mEvBase(evBase), mTxSequence(0), receiveSequence(0, receiveSequenceHistorySize), mAcceptableCiphers(1U << cryptodto::CryptoDtoMode::CryptoModeChaCha20Poly1305), mDtoHandlers(), mLastErrno(0) {
    mDatagramRxBuffer = new unsigned char[maxPermittedDatagramSize];
    mReactorThread.start(mPocoSocketReactor);
}

UDPChannel::~UDPChannel() {
    close();
    mPocoSocketReactor.stop();
    mReactorThread.join();
    delete[] mDatagramRxBuffer;
    mDatagramRxBuffer = nullptr;
}

void UDPChannel::registerDtoHandler(const string &dtoName, std::function<void(const unsigned char *data, size_t len)> callback) {
    mDtoHandlers[dtoName] = callback;
}

void UDPChannel::readCallback(const Poco::AutoPtr<Poco::Net::ReadableNotification> &notification) {
    Poco::Net::SocketAddress sender;
    int dgSize = mPocoUDPSocket.receiveFrom(mDatagramRxBuffer, maxPermittedDatagramSize, sender);

    if (dgSize > maxPermittedDatagramSize) {
        LOG("udpchannel:readCallback", "recv'd datagram %d bytes, exceeding configured maximum of %d", dgSize, maxPermittedDatagramSize);
        return;
    }
    if (dgSize == 0) {
        LOG("udpchannel:readCallback", "recv'd zero-length datagram.  Discarding");
        return;
    }
    if (dgSize < 0) {
        LOG("udpchannel:readCallback", "recv failed: %s", mPocoUDPSocket.impl()->socketError());
        return;
    }

    std::string      channelTag, dtoName;
    sequence_t       seq;
    msgpack::sbuffer dtoBuf;
    CryptoDtoMode    cipherMode;

    if (!Decapsulate(mDatagramRxBuffer, dgSize, channelTag, seq, cipherMode, dtoName, dtoBuf)) {
        LOG("udpchannel:readCallback", "recv'd invalid cryptodto frame.  Discarding");
        return;
    }
    if (!RxModeEnabled(cipherMode)) {
        LOG("udpchannel:readCallback", "got frame encrypted with undesired mode");
        return;
    }
    if (channelTag != ChannelTag) {
        LOG("udpchannel:readCallback", "recv'd with invalid Tag.  Discarding");
        return;
    }
    auto rxOk = receiveSequence.Received(seq);
    switch (rxOk) {
        case ReceiveOutcome::Before:
            LOG("udpchannel:readCallback", "recv'd duplicate sequence %d.  Discarding.", seq);
            return;
        case ReceiveOutcome::OK:
            break;
        case ReceiveOutcome::Overflow:
            break;
    }
    // validate that the packet has a valid payload.
    if (dtoBuf.size() < 2) {
        LOG("udpchannel:readCallback", "internal dto had bad length (too short)");
        return;
    }
    uint16_t dtoSize;
    ::memcpy(&dtoSize, dtoBuf.data(), 2);
    if (dtoSize != dtoBuf.size() - 2) {
        LOG("udpchannel:readCallback", "internal dto had bad length (length encoded mismatched datagram size)");
        return;
    }
    auto dtoIter = mDtoHandlers.find(dtoName);
    if (dtoIter == mDtoHandlers.end()) {
        LOG("udpchannel:readCallback", "no handler for packet-type %s", dtoName.c_str());
        return;
    } else {
        if (dtoBuf.size() == 2) {
            dtoIter->second(nullptr, 0);
        } else {
            dtoIter->second(reinterpret_cast<const unsigned char *>(dtoBuf.data()) + 2, dtoBuf.size() - 2);
        }
    }
}

bool UDPChannel::open() {
    mIsOpen = false;
    if (mAddress.empty()) {
        LOG("udpchannel", "tried to open without address set");
        return false;
    }
    Poco::Net::SocketAddress socketAddress(mAddress.c_str());
    std::string              ipAddress = socketAddress.host().toString();
    Poco::UInt16             port      = socketAddress.port();

    Poco::Net::SocketAddress bindAddress(Poco::Net::IPAddress(), 0);

    // Check if port is within the valid range
    if (port < 1 || port > 65535) {
        LOG("udpchannel", "Invalid port number");
        return false;
    }

    try {
        mPocoUDPSocket = Poco::Net::DatagramSocket();
        mPocoUDPSocket.bind(bindAddress, true);
        mPocoUDPSocket.setBlocking(false);
        mPocoUDPSocket.connect(socketAddress);
        mPocoSocketReactor.addEventHandler(mPocoUDPSocket, Poco::NObserver<UDPChannel, Poco::Net::ReadableNotification>(*this, &UDPChannel::readCallback));
        mIsOpen = true;
        return true;
    } catch (const Poco::Exception &e) {
        LOG("udpchannel", "Couldn't create UDP socket: %s", e.displayText().c_str());
        return false;
    }

    return false;
}

void UDPChannel::close() {
    if (mPocoUDPSocket.impl()) {
        mPocoSocketReactor.removeEventHandler(mPocoUDPSocket, Poco::NObserver<UDPChannel, Poco::Net::ReadableNotification>(*this, &UDPChannel::readCallback));
        mPocoUDPSocket.close();
    }
    mIsOpen = false;
    receiveSequence.reset();
}

void UDPChannel::setAddress(const std::string &address) {
    mAddress = address;
}

bool UDPChannel::isOpen() const {
    return mIsOpen && mPocoUDPSocket.impl();
}

void UDPChannel::enableRxMode(CryptoDtoMode mode) {
    if (mode >= CryptoModeLast) {
        return;
    }
    unsigned int mask = 1U << mode;

    mAcceptableCiphers |= mask;
}

void UDPChannel::disableRxMode(CryptoDtoMode mode) {
    if (mode >= CryptoModeLast) {
        return;
    }
    unsigned int mask = 1U << mode;

    mAcceptableCiphers &= ~mask;
}

bool UDPChannel::RxModeEnabled(CryptoDtoMode mode) const {
    if (mode >= CryptoModeLast) {
        return false;
    }
    unsigned int mask = 1U << mode;
    return mask == (mAcceptableCiphers & mask);
}

void UDPChannel::unregisterDtoHandler(const std::string &dtoName) {
    mDtoHandlers.erase(dtoName);
}

int UDPChannel::getLastErrno() const {
    return mLastErrno;
}

void UDPChannel::setChannelConfig(const dto::ChannelConfig &config) {
    // if the channel keys change, we need to reset our rx expected sequence as the cipher
    // has probably restarted.  We do not need to reset tx since the other end will deal.
    if (::memcmp(aeadReceiveKey, config.AeadReceiveKey, aeadModeKeySize) != 0) {
        receiveSequence.reset();
    }
    Channel::setChannelConfig(config);
}
