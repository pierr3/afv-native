#include "afv-native/afv/APISession.h"
#include "afv-native/http/TransferManager.h"
#include "http/loopback_server.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <ctime>
#include <jwt/jwt.hpp>
#include <mutex>
#include <string>
#include <thread>

using namespace afv_native;
using namespace afv_native::afv;

namespace {
    /** Mints the unsigned token APISession expects: it decodes with
     * algorithms({"none"}) and verify(false), and only reads the exp claim.
     */
    std::string mintToken(int secondsFromNow) {
        jwt::jwt_object obj {jwt::params::algorithm("none"), jwt::params::secret("")};
        obj.add_claim("exp", static_cast<uint64_t>(std::time(nullptr) + secondsFromNow));
        return obj.signature();
    }

    class Counter {
      public:
        void bump() {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                ++mCount;
            }
            mCv.notify_all();
        }

        bool waitFor(int target, std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCv.wait_for(lock, timeout, [&] { return mCount >= target; });
        }

        int get() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCount;
        }

      private:
        std::mutex              mMutex;
        std::condition_variable mCv;
        int                     mCount = 0;
    };
} // namespace

TEST_CASE("the session re-authenticates when the token nears expiry", "[apisession]") {
    // exp 62s out puts the refresh timer at (62 - 60) * 1000 = 2s, so a couple
    // of full refresh cycles complete inside this test rather than in an hour.
    Counter authHits;

    afv_test::LoopbackServer server([&](const std::string &method, const std::string &path,
                                        const std::string &, Poco::Net::HTTPServerResponse &resp) {
        if (path == "/api/v1/auth" && method == "POST") {
            authHits.bump();
            resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            resp.setContentType("text/plain");
            resp.send() << mintToken(62);
            return;
        }
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        resp.send() << "";
    });

    http::TransferManager tm;
    APISession            session(tm, server.url(""), "afv-native-test");
    session.setUsername("tester");
    session.setPassword("hunter2");

    Counter runningStates;
    session.StateCallback.addCallback(nullptr, [&](APISessionState state) {
        if (state == APISessionState::Running) {
            runningStates.bump();
        }
    });

    session.Connect();

    // Wait on the Running transitions rather than the auth requests: a refresh
    // moves Running -> Reconnecting -> Running, so the request arriving at the
    // server does not mean the cycle has finished.  Three of them means the
    // initial login plus two timer-driven refreshes that each re-armed.
    REQUIRE(runningStates.waitFor(3, std::chrono::seconds(30)));
    REQUIRE(authHits.get() >= 3);
    REQUIRE(session.getState() == APISessionState::Running);

    session.Disconnect();
    REQUIRE(session.getState() == APISessionState::Disconnected);

    // Disconnect disables the timer, so no further auth requests arrive.
    const int settled = authHits.get();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    REQUIRE(authHits.get() == settled);
}

TEST_CASE("a rejected login reports the matching session error", "[apisession]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &,
                                       const std::string &, Poco::Net::HTTPServerResponse &resp) {
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_FORBIDDEN);
        resp.send() << "";
    });

    http::TransferManager tm;
    APISession            session(tm, server.url(""), "afv-native-test");
    session.setUsername("tester");
    session.setPassword("wrong");

    Counter errors;
    session.StateCallback.addCallback(nullptr, [&](APISessionState state) {
        if (state == APISessionState::Error) {
            errors.bump();
        }
    });

    session.Connect();

    REQUIRE(errors.waitFor(1, std::chrono::seconds(30)));
    REQUIRE(session.getLastError() == APISessionError::RejectedCredentials);
}

TEST_CASE("an unreachable API server reports a connection error", "[apisession]") {
    http::TransferManager tm;
    // Port 1 on loopback refuses immediately.
    APISession session(tm, "http://127.0.0.1:1", "afv-native-test");
    session.setUsername("tester");
    session.setPassword("hunter2");

    Counter errors;
    session.StateCallback.addCallback(nullptr, [&](APISessionState state) {
        if (state == APISessionState::Error) {
            errors.bump();
        }
    });

    session.Connect();

    REQUIRE(errors.waitFor(1, std::chrono::seconds(30)));
    REQUIRE(session.getLastError() == APISessionError::ConnectionError);
}

TEST_CASE("VCCS stations retain the order returned by the API", "[apisession][vccs]") {
    dto::Station ordinaryStation;
    REQUIRE_FALSE(ordinaryStation.VccsOrder.has_value());

    afv_test::LoopbackServer server([](const std::string &method, const std::string &path,
                                       const std::string &, Poco::Net::HTTPServerResponse &resp) {
        if (path == "/api/v1/auth" && method == "POST") {
            resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            resp.setContentType("text/plain");
            resp.send() << mintToken(3600);
            return;
        }
        if (path == "/api/v1/stations/byName/EKCH_W_APP/vccsStations" && method == "GET") {
            resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
            resp.setContentType("application/json");
            resp.send() << R"([
                {"id":"1","name":"EKCH_W_APP","frequency":119805000,"frequencyAlias":0},
                {"id":"2","name":"EKCH_R_DEP","frequency":120255000,"frequencyAlias":0},
                {"id":"3","name":"EKCH_O_APP","frequency":118155000,"frequencyAlias":0},
                {"id":"4","name":"EKCH_K_DEP","frequency":124980000,"frequencyAlias":0}
            ])";
            return;
        }
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_NOT_FOUND);
        resp.send() << "";
    });

    http::TransferManager tm;
    APISession            session(tm, server.url(""), "afv-native-test");
    session.setUsername("tester");
    session.setPassword("hunter2");

    Counter runningStates;
    session.StateCallback.addCallback(nullptr, [&](APISessionState state) {
        if (state == APISessionState::Running) {
            runningStates.bump();
        }
    });

    std::map<std::string, dto::Station> stations;
    Counter                             vccsResponses;
    session.StationVccsCallback.addCallback(
        nullptr, [&](const std::string &, std::map<std::string, dto::Station> response) {
            stations = std::move(response);
            vccsResponses.bump();
        });

    session.Connect();
    REQUIRE(runningStates.waitFor(1, std::chrono::seconds(30)));

    session.requestStationVccs("EKCH_W_APP");
    REQUIRE(vccsResponses.waitFor(1, std::chrono::seconds(30)));

    REQUIRE(stations.size() == 4);
    REQUIRE(stations.at("EKCH_W_APP").VccsOrder == 0);
    REQUIRE(stations.at("EKCH_R_DEP").VccsOrder == 1);
    REQUIRE(stations.at("EKCH_O_APP").VccsOrder == 2);
    REQUIRE(stations.at("EKCH_K_DEP").VccsOrder == 3);

    session.Disconnect();
}

