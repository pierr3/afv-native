/* afv/APISession.cpp
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

#include "afv-native/afv/APISession.h"
#include "afv-native/afv/VoiceSession.h"
#include "afv-native/afv/dto/AuthRequest.h"
#include "afv-native/afv/dto/PostCallsignResponse.h"
#include "afv-native/afv/params.h"
#include <cassert>
#include <functional>
#include <jwt/jwt.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>

using namespace ::afv_native::afv;
using namespace ::afv_native;
using json = nlohmann::json;

APISession::APISession(http::TransferManager &tm, std::string baseUrl, std::string clientName):
    StateCallback(), AliasUpdateCallback(), mTransferManager(tm), mBaseURL(std::move(baseUrl)), mUsername(), mPassword(), mClientName(std::move(clientName)), mBearerToken(), mRefreshTokenTimer(std::bind(&APISession::Connect, this)), mLastError(APISessionError::NoError), mState(APISessionState::Disconnected), StationTransceiversUpdateCallback(), StationVccsCallback(), StationSearchCallback() {
}

void APISession::Connect() {
    dto::AuthRequest ar(mUsername, mPassword, mClientName);

    /* start the authentication request */
    http::Request req(mBaseURL + "/api/v1/auth", http::Method::POST);
    req.setBody(json(ar));
    // Tighter than the default: the refresh runs 60s before the live token
    // expires, so this has to fail inside that window to be reportable.
    req.setTimeouts(5, 15);

    mTransferManager.submit(std::move(req), [this](const http::Response &resp) {
        this->_authenticationCallback(resp);
    });

    /* update our internal state */
    switch (mState.load()) {
        case APISessionState::Disconnected:
            setState(APISessionState::Connecting);
            break;
        case APISessionState::Running:
            setState(APISessionState::Reconnecting);
            break;
        default:
            break;
    }
}

void afv::APISession::Disconnect() {
    mRefreshTokenTimer.disable();
    mBearerToken = "";
    setState(APISessionState::Disconnected);
}

const std::string &APISession::getUsername() const {
    return mUsername;
}

void APISession::setUsername(const std::string &username) {
    mUsername = username;
}

void APISession::setPassword(const std::string &password) {
    mPassword = password;
}

void APISession::_authenticationCallback(const http::Response &resp) {
    if (resp.ok && resp.statusCode == 200) {
        mBearerToken = resp.body;
        if (mBearerToken.empty()) {
            LOG("APISession", "No Token Received");
            mBearerToken = "";
            raiseError(APISessionError::InvalidAuthToken);
            return;
        }
        try {
            using namespace jwt::params;
            std::error_code ec;
            auto dec_token = jwt::decode(mBearerToken, algorithms({"none"}), ec, verify(false));
            if (ec) {
                LOG("APISession", "couldn't parse bearer token: %s",
                    ec.message().c_str());
                mBearerToken = "";
                raiseError(APISessionError::InvalidAuthToken);
                return;
            } else {
                if (dec_token.payload().has_claim("exp")) {
                    const time_t expiry = dec_token.payload().get_claim_value<uint64_t>("exp");
                    const int timeRemaining = expiry - ::time(nullptr);
                    if (timeRemaining <= 60) {
                        LOG("APISession", "token TTL (%d) is <= 60s.  Please check your system clock.", timeRemaining);
                        mBearerToken = "";
                        raiseError(APISessionError::AuthTokenExpiryTimeInPast);
                        return;
                    }
                    LOG("APISession", "API Token Expires in %d seconds", expiry - time(nullptr));
                    // refresh 1 minute before token expiry.
                    mRefreshTokenTimer.enable((timeRemaining - 60) * 1000);
                } else {
                    LOG("APISession", "no expiry claim - assuming 1 hour.",
                        ec.message().c_str());
                    mRefreshTokenTimer.enable(59 * 60 * 1000); // refresh in 59 minutes.
                }
            }
        } catch (const std::exception &e) {
            // if we failed in here, carp about it, and use 6 hours.
            LOG("APISession", "Couldn't parse Bearer Token to get expiry time: %s", e.what());
            mBearerToken = "";
            raiseError(APISessionError::InvalidAuthToken);
            return;
        }
        setState(APISessionState::Running);
    } else {
        // This is a failure during auth, which is grounds to handle it as
        // if it were an immediate disconnect.
        mBearerToken = "";
        if (!resp.ok) {
            LOG("APISession", "http error during login: %s", resp.error.c_str());
            raiseError(APISessionError::ConnectionError);
        } else {
            LOG("APISession", "got error from API server: Response Code %ld", resp.statusCode);
            switch (resp.statusCode) {
                case 400:
                    raiseError(APISessionError::BadRequestOrClientIncompatible);
                    break;
                case 401:
                    raiseError(APISessionError::BadPassword);
                    break;
                case 403:
                    raiseError(APISessionError::RejectedCredentials);
                    break;
                default:
                    raiseError(APISessionError::OtherRequestError);
                    break;
            }
        }
        setState(APISessionState::Disconnected);
    }
}

void APISession::setAuthenticationFor(http::Request &r) {
    assert(!mBearerToken.empty());
    r.setHeader("Authorization", "Bearer " + mBearerToken);
}

http::TransferManager &APISession::getTransferManager() const {
    return mTransferManager;
}

APISessionState APISession::getState() const {
    return mState.load();
}

const std::string &APISession::getBaseUrl() const {
    return mBaseURL;
}

void APISession::setBaseUrl(std::string newUrl) {
    mBaseURL = std::move(newUrl);
}

void APISession::setState(APISessionState newState) {
    if (newState != mState.load()) {
        mState.store(newState);
        StateCallback.invokeAll(newState);
    }
}

void APISession::raiseError(APISessionError error) {
    mState.store(APISessionState::Disconnected);
    mLastError.store(error);
    StateCallback.invokeAll(APISessionState::Error);
}

APISessionError APISession::getLastError() const {
    return mLastError.load();
}

void APISession::getStation(std::string stdName) {
    if (mState.load() != APISessionState::Running) {
        return;
    }

    http::Request req(mBaseURL + "/api/v1/stations/byName/" + stdName, http::Method::GET);
    setAuthenticationFor(req);
    mTransferManager.submit(std::move(req), [this, stdName](const http::Response &resp) {
        this->_getStationCallback(resp, stdName);
    });
}

void APISession::updateStationAliases() {
    http::Request req(mBaseURL + "/api/v1/stations/aliased", http::Method::GET);
    setAuthenticationFor(req);
    mTransferManager.submit(std::move(req), [this](const http::Response &resp) {
        this->_stationsCallback(resp);
    });
}

void APISession::_getStationCallback(const http::Response &resp, std::string stationName) {
    std::pair<std::string, dto::Station> ret;

    if (resp.ok && resp.statusCode == 200) {
        auto jsReturn = resp.json();

        bool                                 found = false;

        try {
            if (!jsReturn.is_object()) {
                LOG("APISession", "station data returned wasn't an object.  Ignoring.");
                found = false;
                ret   = {stationName, dto::Station()};
                StationSearchCallback.invokeAll(false, ret);
                return;
            }
            dto::Station s;
            jsReturn.get_to(s);
            found = true;
            ret   = {stationName, s};
        } catch (std::exception &e) {
            LOG("APISession", "couldn't decode station data: %s", e.what());
        }

        StationSearchCallback.invokeAll(found, ret);
    } else {
        if (!resp.ok) {
            LOG("APISession", "http error during get station retrieval: %s", resp.error.c_str());
        } else {
            // We log the error but also return that we did not find the station if 404
            if (resp.statusCode == 404) {
                ret = {stationName, dto::Station()};
                StationSearchCallback.invokeAll(false, ret);
            }
            LOG("APISession", "got error from API server get station: Response Code %ld", resp.statusCode);
        }
    }
}

void APISession::_stationsCallback(const http::Response &resp) {
    if (resp.ok && resp.statusCode == 200) {
        auto jsReturn = resp.json();

        if (!jsReturn.is_array()) {
            LOG("APISession", "station data returned wasn't an array.  Ignoring.");
        } else {
            mAliasedStations.clear();
            for (const auto &sJson: jsReturn) {
                dto::Station s;
                try {
                    sJson.get_to(s);
                    mAliasedStations.emplace_back(s);
                } catch (nlohmann::json::exception &e) {
                    LOG("APISession", "couldn't decode station alias: %s", e.what());
                }
            }
            LOG("APISession", "got %d station aliases.", mAliasedStations.size());
            AliasUpdateCallback.invokeAll();
        }
    } else {
        if (!resp.ok) {
            LOG("APISession", "http error during alias retrieval: %s", resp.error.c_str());
            // raiseError(APISessionError::ConnectionError);
        } else {
            LOG("APISession", "got error from API server getting aliases: Response Code %ld", resp.statusCode);
        }
    }
}

std::vector<dto::Station> APISession::getStationAliases() const {
    return mAliasedStations;
}

void APISession::requestStationTransceivers(std::string stdName) {
    if (mState.load() != APISessionState::Running) {
        return;
    }

    http::Request req(mBaseURL + "/api/v1/stations/byName/" + stdName + "/transceivers/allDistinctObeyExclusions",
                      http::Method::GET);
    setAuthenticationFor(req);
    mTransferManager.submit(std::move(req), [this, stdName](const http::Response &resp) {
        this->_stationTransceiversCallback(resp, stdName);
    });
}

void APISession::requestStationVccs(std::string stdName) {
    if (mState.load() != APISessionState::Running) {
        return;
    }

    http::Request req(mBaseURL + "/api/v1/stations/byName/" + stdName + "/vccsStations", http::Method::GET);
    setAuthenticationFor(req);
    mTransferManager.submit(std::move(req), [this, stdName](const http::Response &resp) {
        this->_stationVccsCallback(resp, stdName);
    });
}

void APISession::_stationVccsCallback(const http::Response &resp, std::string stdName) {
    if (resp.ok && resp.statusCode == 200) {
        auto jsReturn = resp.json();

        std::map<std::string, dto::Station> ret;

        if (!jsReturn.is_array()) {
            LOG("APISession", "station vccs data returned wasn't an array.  Ignoring.");
        } else {
            for (const auto &sJson: jsReturn) {
                try {
                    dto::Station s;
                    sJson.get_to(s);
                    ret.insert({sJson["name"].get<std::string>(), s});
                } catch (nlohmann::json::exception &e) {
                    LOG("APISession", "couldn't decode station vccs: %s", e.what());
                }
            }
        }

        StationVccsCallback.invokeAll(stdName, ret);
    } else {
        if (!resp.ok) {
            LOG("APISession", "http error during station vccs retrieval: %s", resp.error.c_str());
        } else {
            LOG("APISession", "got error from API server getting vccs: Response Code %ld", resp.statusCode);
        }
    }
}

void APISession::_stationTransceiversCallback(const http::Response &resp, std::string stdName) {
    if (resp.ok && resp.statusCode == 200) {
        auto jsReturn = resp.json();

        if (!jsReturn.is_array()) {
            LOG("APISession", "station transceivers data returned wasn't an array.  Ignoring.");
        } else {
            {
                // this callback runs on the transfer manager's thread while
                // clients read the map from their own threads.
                std::lock_guard<std::mutex> mapLock(mStationTransceiversLock);
                mStationTransceivers[stdName].clear();

                for (const auto &sJson: jsReturn) {
                    dto::StationTransceiver st;
                    try {
                        sJson.get_to(st);
                        mStationTransceivers[stdName].emplace_back(st);
                    } catch (nlohmann::json::exception &e) {
                        LOG("APISession", "couldn't decode station transceivers: %s", e.what());
                    }
                }
                LOG("APISession", "got %d station transceivers for station %s.",
                    mStationTransceivers[stdName].size(), stdName.c_str());
            }
            StationTransceiversUpdateCallback.invokeAll(stdName);
        }
    } else {
        if (!resp.ok) {
            LOG("APISession", "http error during station receivers retrieval: %s", resp.error.c_str());
            // raiseError(APISessionError::ConnectionError);
        } else {
            LOG("APISession", "got error from API server getting station transceivers: Response Code %ld", resp.statusCode);
        }
    }
}

std::map<std::string, std::vector<dto::StationTransceiver>> APISession::getStationTransceivers() const {
    std::lock_guard<std::mutex> mapLock(mStationTransceiversLock);
    return mStationTransceivers;
}
