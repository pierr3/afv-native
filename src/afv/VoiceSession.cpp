/* afv/VoiceSession.cpp
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

#include "afv-native/afv/VoiceSession.h"
#include "afv-native/Log.h"
#include "afv-native/afv/APISession.h"
#include "afv-native/afv/dto/CrossCoupleGroup.h"
#include "afv-native/afv/dto/Transceiver.h"
#include "afv-native/afv/dto/voice_server/Heartbeat.h"
#include "afv-native/afv/params.h"
#include "afv-native/cryptodto/UDPChannel.h"
#include "afv-native/http/Request.h"
#include <nlohmann/json.hpp>

using namespace afv_native::afv;
using namespace afv_native;
using json = nlohmann::json;

VoiceSession::VoiceSession(APISession &session, const std::string &callsign) try:    
    mCallsign(callsign), 
    mBaseUrl(""), 
    mVoiceSessionSetupRequest("", http::Method::POST, json()), 
    mVoiceSessionTeardownRequest("", http::Method::DEL, json()), 
    mTransceiverUpdateRequest("", http::Method::POST, json()), 
    mCrossCoupleGroupUpdateRequest("", http::Method::POST, json()),
    mChannel(), 
    mHeartbeatTimer(session.getEventBase(), std::bind(&VoiceSession::sendHeartbeatCallback, this)), 
    mLastHeartbeatReceived(0),
    mHeartbeatTimeout(session.getEventBase(), std::bind(&VoiceSession::heartbeatTimedOut, this)), 
    mSession(session), 
    mLastError(VoiceSessionError::NoError) {
    LOG("VoiceSession","Entry");
    
    mSessionType = VoiceSessionType::Pilot;
    updateBaseUrl();
    LOG("VoiceSession","Created");
}
catch (std::exception &e) {
    LOG("VoiceSession","Exception: %s type: %s",e.what(),typeid(e).name());
}

VoiceSession::~VoiceSession() {
    mHeartbeatTimer.disable();
    mHeartbeatTimeout.disable();
    mChannel.close();
    mVoiceSessionSetupRequest.reset();
}

bool VoiceSession::Connect() {
    // we cannot start the voice session if the API session is in any state OTHER than running...
    if (mSession.getState() != APISessionState::Running) {
        return false;
    }
    mVoiceSessionSetupRequest.reset();
    // fix up the request URL
    updateBaseUrl();
    mSession.setAuthenticationFor(mVoiceSessionSetupRequest);
    mVoiceSessionSetupRequest.setCompletionCallback(std::bind(&VoiceSession::voiceSessionSetupRequestCallback, this, std::placeholders::_1, std::placeholders::_2));
    auto &transferManager = mSession.getTransferManager();
    mVoiceSessionSetupRequest.shareState(transferManager);
    mVoiceSessionSetupRequest.doAsync(transferManager);
    return true;
}

void VoiceSession::voiceSessionSetupRequestCallback(http::Request *req, bool success) {
    if (success) {
        if (req->getStatusCode() == 200) {
            auto *restreq = dynamic_cast<http::RESTRequest *>(req);
            assert(restreq != nullptr);
            try {
                auto                      j = restreq->getResponse();
                dto::PostCallsignResponse cresp;
                j.get_to(cresp);
                if (!setupSession(cresp)) {
                    failSession();
                }
            } catch (json::exception &e) {
                LOG("voicesession", "exception parsing voice session setup: %s", e.what());
                mLastError = VoiceSessionError::BadResponseFromAPIServer;
                failSession();
            }
        } else {
            LOG("voicesession", "request for voice session failed: got status %d", req->getStatusCode());
            mLastError = VoiceSessionError::BadResponseFromAPIServer;
            failSession();
        }
    } else {
        LOG("voicesession", "request for voice session failed: got internal error %s",
            req->getCurlError().c_str());
        LOG("voicesession","Request: %s",req->getUrl().c_str());
        mLastError = VoiceSessionError::BadResponseFromAPIServer;
        failSession();
    }
}

bool VoiceSession::setupSession(const dto::PostCallsignResponse &cresp) {
    mChannel.close();
    mChannel.setAddress(cresp.VoiceServer.AddressIpV4);
    mChannel.setChannelConfig(cresp.VoiceServer.ChannelConfig);
    if (!mChannel.open()) {
        LOG("VoiceSession:setupSession", "unable to open UDP session");
        mLastError = VoiceSessionError::UDPChannelError;
        return false;
    }
    mVoiceSessionSetupRequest.reset();
    mVoiceSessionTeardownRequest.reset();
    mLastHeartbeatReceived = util::monotime_get();
    mHeartbeatTimer.enable(afvHeartbeatIntervalMs);
    mHeartbeatTimeout.enable(afvHeartbeatTimeoutMs);
    mChannel.registerDtoHandler("HA", [this](const unsigned char *data, size_t len) {
        this->receivedHeartbeat();
    });
    mLastError = VoiceSessionError::NoError;
    StateCallback.invokeAll(VoiceSessionState::Connected);

    // now that everything's been invoked, attach our callback for reconnect handling
    mSession.StateCallback.addCallback(this, std::bind(&VoiceSession::sessionStateCallback, this, std::placeholders::_1));
    return true;
}

void VoiceSession::failSession() {
    mHeartbeatTimer.disable();
    mHeartbeatTimeout.disable();
    mChannel.close();
    mVoiceSessionSetupRequest.reset();
    // before we invoke state callbacks, remove our session handler so we don't get recursive loops.
    mSession.StateCallback.removeCallback(this);
    if (mLastError != VoiceSessionError::NoError) {
        StateCallback.invokeAll(VoiceSessionState::Error);
    } else {
        StateCallback.invokeAll(VoiceSessionState::Disconnected);
    }
}

void VoiceSession::sendHeartbeatCallback() {
    dto::Heartbeat hbDto(mCallsign);
    if (mChannel.isOpen()) {
        mChannel.sendDto(hbDto);
        mHeartbeatTimer.enable(afvHeartbeatIntervalMs);
    }
}

void VoiceSession::receivedHeartbeat() {
    mLastHeartbeatReceived = util::monotime_get();
    mHeartbeatTimeout.disable();
    mHeartbeatTimeout.enable(afvHeartbeatTimeoutMs);
}

void VoiceSession::heartbeatTimedOut() {
    util::monotime_t now = util::monotime_get();
    LOG("voicesession", "heartbeat timeout - %d ms elapsed - disconnecting", now - mLastHeartbeatReceived);
    mLastError = VoiceSessionError::Timeout;
    Disconnect(true, true);
}

void VoiceSession::Disconnect(bool do_close, bool reconnect) {
    if (do_close) {
        mVoiceSessionTeardownRequest.reset();
        mVoiceSessionTeardownRequest.setUrl(mBaseUrl);
        mSession.setAuthenticationFor(mVoiceSessionTeardownRequest);
        // because we're likely going to get discarded by our owner when this function returns, we
        // need to hold onto a shared_ptr reference to prevent cleanup until *AFTER* this callback completes.
        auto &transferManager = mSession.getTransferManager();
        mVoiceSessionTeardownRequest.setCompletionCallback([](http::Request *req, bool success) mutable {
            if (success) {
                if (req->getStatusCode() != 200) {
                    LOG("VoiceSession:Disconnect", "Callsign Dereg Failed.  Status Code: %d", req->getStatusCode());
                }
            } else {
                LOG("VoiceSession:Disconnect", "Callsign Dereg Failed.  Internal Error: %s",
                    req->getCurlError().c_str());
            }
        });
        // and now schedule this request to be performed.
        mVoiceSessionTeardownRequest.shareState(transferManager);
        mVoiceSessionTeardownRequest.doAsync(transferManager);
    }
    failSession();

    if (reconnect) {
        mSession.Connect();
    }
}

void VoiceSession::postTransceiverUpdate(const std::vector<dto::Transceiver> &txDto, std::function<void(http::Request *, bool)> callback) {
    updateBaseUrl();
    mTransceiverUpdateRequest.reset();
    mSession.setAuthenticationFor(mTransceiverUpdateRequest);

    // only send the transceivers that have a valid frequency (read: not zero)
    std::vector<dto::Transceiver> filteredDto;
    std::copy_if(txDto.begin(), txDto.end(), std::back_inserter(filteredDto), [](dto::Transceiver t) {
        return t.Frequency > 0;
    });

    mTransceiverUpdateRequest.setRequestBody(filteredDto);
    mTransceiverUpdateRequest.setCompletionCallback(callback);
    // and now schedule this request to be performed.
    auto &transferManager = mSession.getTransferManager();
    mTransceiverUpdateRequest.shareState(transferManager);
    mTransceiverUpdateRequest.doAsync(transferManager);

    LOG("VoiceSession", "postTransceiverUpdate");
}

void VoiceSession::postCrossCoupleGroupUpdate(const std::vector<dto::CrossCoupleGroup> &ccDto, std::function<void(http::Request *, bool)> callback) {
    // if(mSessionType!=VoiceSessionType::ATC) return;
    updateBaseUrl();
    mCrossCoupleGroupUpdateRequest.reset();
    mSession.setAuthenticationFor(mCrossCoupleGroupUpdateRequest);
    mCrossCoupleGroupUpdateRequest.setRequestBody(ccDto);
    mCrossCoupleGroupUpdateRequest.setCompletionCallback(callback);
    // and now schedule this request to be performed.
    auto &transferManager = mSession.getTransferManager();
    mCrossCoupleGroupUpdateRequest.shareState(transferManager);
    mCrossCoupleGroupUpdateRequest.doAsync(transferManager);
}

bool VoiceSession::isConnected() const {
    return mChannel.isOpen();
}

VoiceSessionType VoiceSession::type() const {
    return mSessionType;
}

void VoiceSession::setType(VoiceSessionType inType) {
    mSessionType = inType;
}

void VoiceSession::setCallsign(const std::string &newCallsign) {
    if (!mChannel.isOpen()) {
        mCallsign = newCallsign;
    }
}

afv_native::cryptodto::UDPChannel &VoiceSession::getUDPChannel() {
    return mChannel;
}

void VoiceSession::updateBaseUrl() {
    mBaseUrl = mSession.getBaseUrl() + "/api/v1/users/" + mSession.getUsername() + "/callsigns/" + mCallsign;
    mVoiceSessionSetupRequest.setUrl(mBaseUrl);
    mVoiceSessionTeardownRequest.setUrl(mBaseUrl);
    mTransceiverUpdateRequest.setUrl(mBaseUrl + "/transceivers");
    mCrossCoupleGroupUpdateRequest.setUrl(mBaseUrl + "/crossCoupleGroups");
}

VoiceSessionError VoiceSession::getLastError() const {
    return mLastError;
}

void VoiceSession::sessionStateCallback(APISessionState state) {
    switch (state) {
        case afv::APISessionState::Disconnected:
        case afv::APISessionState::Error:
            if (isConnected()) {
                failSession();
            }
            break;
        default:
            break;
    }
}
