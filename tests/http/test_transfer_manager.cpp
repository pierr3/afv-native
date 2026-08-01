#include "afv-native/http/TransferManager.h"
#include "http/loopback_server.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <set>
#include <string>
#include <thread>

using namespace afv_native::http;

namespace {
    /** Blocks until a target number of callbacks have fired, or the deadline
     * passes.  Sleeping on a fixed duration would make these tests flaky on a
     * loaded machine.
     */
    class Latch {
      public:
        void signal() {
            {
                std::lock_guard<std::mutex> lock(mMutex);
                ++mCount;
            }
            mCv.notify_all();
        }

        bool wait(int target, std::chrono::milliseconds timeout) {
            std::unique_lock<std::mutex> lock(mMutex);
            return mCv.wait_for(lock, timeout, [&] { return mCount >= target; });
        }

        int count() {
            std::lock_guard<std::mutex> lock(mMutex);
            return mCount;
        }

      private:
        std::mutex              mMutex;
        std::condition_variable mCv;
        int                     mCount = 0;
    };

    /** Echoes the request path back as the body. */
    std::unique_ptr<afv_test::LoopbackServer> makeEchoServer() {
        return std::make_unique<afv_test::LoopbackServer>(
            [](const std::string &, const std::string &path, const std::string &,
               Poco::Net::HTTPServerResponse &resp) {
                resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
                resp.setContentType("text/plain");
                resp.send() << path;
            });
    }
} // namespace

TEST_CASE("submit delivers a response to the callback", "[tm]") {
    auto            server = makeEchoServer();
    TransferManager tm(2);

    Latch    latch;
    Response seen;
    tm.submit(Request(server->url("/one"), Method::GET), [&](const Response &r) {
        seen = r;
        latch.signal();
    });

    REQUIRE(latch.wait(1, std::chrono::seconds(10)));
    REQUIRE(seen.ok);
    REQUIRE(seen.statusCode == 200);
    REQUIRE(seen.body == "/one");
}

TEST_CASE("fifty concurrent submits all complete exactly once", "[tm]") {
    auto            server = makeEchoServer();
    TransferManager tm(4);

    constexpr int         kCount = 50;
    Latch                 latch;
    std::mutex            mutex;
    std::set<std::string> bodies;

    for (int i = 0; i < kCount; ++i) {
        tm.submit(Request(server->url("/n" + std::to_string(i)), Method::GET), [&](const Response &r) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                bodies.insert(r.body);
            }
            latch.signal();
        });
    }

    REQUIRE(latch.wait(kCount, std::chrono::seconds(60)));
    REQUIRE(latch.count() == kCount);
    REQUIRE(bodies.size() == static_cast<size_t>(kCount));
}

TEST_CASE("completion callbacks never run concurrently with each other", "[tm]") {
    auto            server = makeEchoServer();
    TransferManager tm(4);

    std::atomic<int> inFlight {0};
    std::atomic<int> maxObserved {0};
    Latch            latch;

    constexpr int kCount = 30;
    for (int i = 0; i < kCount; ++i) {
        tm.submit(Request(server->url("/x"), Method::GET), [&](const Response &) {
            const int now  = ++inFlight;
            int       prev = maxObserved.load();
            while (now > prev && !maxObserved.compare_exchange_weak(prev, now)) {
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
            --inFlight;
            latch.signal();
        });
    }

    REQUIRE(latch.wait(kCount, std::chrono::seconds(60)));
    REQUIRE(maxObserved.load() == 1);
}

TEST_CASE("a cancelled request does not invoke its callback", "[tm]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &, const std::string &,
                                       Poco::Net::HTTPServerResponse &resp) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        resp.send() << "late";
    });

    TransferManager  tm(2);
    std::atomic<int> fired {0};

    auto handle = tm.submit(Request(server.url("/slow"), Method::GET), [&](const Response &) { ++fired; });
    REQUIRE(handle != 0);
    tm.cancel(handle);

    std::this_thread::sleep_for(std::chrono::seconds(4));
    REQUIRE(fired.load() == 0);
}

TEST_CASE("destroying the manager with requests in flight is clean", "[tm]") {
    afv_test::LoopbackServer server([](const std::string &, const std::string &, const std::string &,
                                       Poco::Net::HTTPServerResponse &resp) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        resp.setStatus(Poco::Net::HTTPResponse::HTTP_OK);
        resp.send() << "ok";
    });

    {
        TransferManager tm(4);
        for (int i = 0; i < 20; ++i) {
            tm.submit(Request(server.url("/d"), Method::GET), [](const Response &) {});
        }
        // The destructor runs here with work outstanding.  It must not hang,
        // crash, or invoke a callback after the manager is gone.
    }
    SUCCEED("manager destroyed without hanging");
}

TEST_CASE("back-to-back requests to the same endpoint both complete", "[tm]") {
    // The old design gave each call site one reusable Request member, so a
    // second call reset() the first out from under itself and that callback
    // never fired.  Submitting is now independent per request.
    auto            server = makeEchoServer();
    TransferManager tm(4);

    Latch                 latch;
    std::mutex            mutex;
    std::set<std::string> bodies;

    for (const auto *station: {"/EGLL", "/EGKK", "/EGSS"}) {
        tm.submit(Request(server->url(station), Method::GET), [&](const Response &r) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                bodies.insert(r.body);
            }
            latch.signal();
        });
    }

    REQUIRE(latch.wait(3, std::chrono::seconds(30)));
    REQUIRE(bodies == std::set<std::string> {"/EGLL", "/EGKK", "/EGSS"});
}

TEST_CASE("more requests than workers all complete", "[tm]") {
    // Excess submissions queue rather than being dropped or overriding.
    auto            server = makeEchoServer();
    TransferManager tm(2);

    constexpr int kCount = 12;
    Latch         latch;

    for (int i = 0; i < kCount; ++i) {
        tm.submit(Request(server->url("/q" + std::to_string(i)), Method::GET),
                  [&](const Response &r) {
                      REQUIRE(r.ok);
                      latch.signal();
                  });
    }

    REQUIRE(latch.wait(kCount, std::chrono::seconds(60)));
    REQUIRE(latch.count() == kCount);
}

TEST_CASE("performSync returns inline without touching the pool", "[tm]") {
    auto            server = makeEchoServer();
    TransferManager tm(1);

    Response r = tm.performSync(Request(server->url("/sync"), Method::GET));
    REQUIRE(r.ok);
    REQUIRE(r.body == "/sync");
}
