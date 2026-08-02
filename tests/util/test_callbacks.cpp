#include "afv-native/event/EventBus.h"
#include "afv-native/util/ChainedCallback.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <string>
#include <thread>
#include <vector>

using afv_native::util::ChainedCallback;

namespace {
    struct AlphaEvent {
        int value = 0;
    };
    struct BetaEvent {
        int value = 0;
    };
} // namespace

TEST_CASE("every subscriber receives the argument intact", "[chainedcallback]") {
    // Regression: invokeAll used to std::forward inside its dispatch loop, so
    // the first callback moved the value out and the rest saw an empty object.
    ChainedCallback<void(std::string)> cb;

    std::vector<std::string> seen;
    std::mutex               mutex;
    auto                     record = [&](std::string s) {
        std::lock_guard<std::mutex> lock(mutex);
        seen.push_back(std::move(s));
    };

    int refA = 0, refB = 0, refC = 0;
    cb.addCallback(&refA, record);
    cb.addCallback(&refB, record);
    cb.addCallback(&refC, record);

    cb.invokeAll("EGLL");

    REQUIRE(seen.size() == 3);
    for (const auto &s: seen) {
        REQUIRE(s == "EGLL");
    }
}

TEST_CASE("compound arguments survive multiple subscribers", "[chainedcallback]") {
    ChainedCallback<void(std::string, std::vector<int>)> cb;

    std::atomic<int> intact {0};
    auto             check = [&](std::string s, std::vector<int> v) {
        if (s == "payload" && v.size() == 3) {
            ++intact;
        }
    };

    int refA = 0, refB = 0;
    cb.addCallback(&refA, check);
    cb.addCallback(&refB, check);

    cb.invokeAll("payload", {1, 2, 3});
    REQUIRE(intact.load() == 2);
}

TEST_CASE("concurrent add, remove and invoke stay consistent", "[chainedcallback]") {
    ChainedCallback<void(std::string)> cb;
    std::atomic<bool>                  stop {false};
    std::atomic<int>                   invocations {0};
    std::atomic<int>                   corrupt {0};

    std::thread invoker([&] {
        while (!stop.load()) {
            cb.invokeAll("stable");
            ++invocations;
        }
    });

    std::vector<std::thread> churn;
    for (int t = 0; t < 4; ++t) {
        churn.emplace_back([&, t] {
            std::vector<int> refs(16);
            while (!stop.load()) {
                for (auto &r: refs) {
                    cb.addCallback(&r, [&](const std::string &s) {
                        if (s != "stable") {
                            ++corrupt;
                        }
                    });
                }
                for (auto &r: refs) {
                    cb.removeCallback(&r);
                }
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop.store(true);
    invoker.join();
    for (auto &t: churn) {
        t.join();
    }

    REQUIRE(invocations.load() > 0);
    REQUIRE(corrupt.load() == 0);
}

TEST_CASE("handler ids are unique across event types", "[eventbus]") {
    // EventStream<T>::nextHandlerId_ is static per T, so two event types each
    // start numbering at 0.  EventBus keys handlerTypes_ on the bare id, so
    // the second registration overwrites the first and the first handler can
    // never be removed.
    auto &bus = afv_native::event::EventBus::Instance();
    bus.Reset();

    std::atomic<int> alphaHits {0};
    std::atomic<int> betaHits {0};

    auto alphaId = bus.AddHandler<AlphaEvent>([&](const AlphaEvent &) { ++alphaHits; });
    auto betaId  = bus.AddHandler<BetaEvent>([&](const BetaEvent &) { ++betaHits; });

    REQUIRE(alphaId != betaId);

    // Removing beta must leave alpha registered.
    REQUIRE(bus.RemoveHandler(betaId));
    bus.OnEvent(AlphaEvent {1});
    bus.OnEvent(BetaEvent {1});

    REQUIRE(alphaHits.load() == 1);
    REQUIRE(betaHits.load() == 0);

    bus.Reset();
}

TEST_CASE("concurrent EventBus dispatch and registration stay consistent", "[eventbus]") {
    auto &bus = afv_native::event::EventBus::Instance();
    bus.Reset();

    std::atomic<bool> stop {false};
    std::atomic<int>  dispatches {0};

    std::thread emitter([&] {
        while (!stop.load()) {
            bus.OnEvent(AlphaEvent {7});
            ++dispatches;
        }
    });

    std::vector<std::thread> churn;
    for (int t = 0; t < 3; ++t) {
        churn.emplace_back([&] {
            while (!stop.load()) {
                auto id = bus.AddHandler<AlphaEvent>([](const AlphaEvent &) {});
                bus.RemoveHandler(id);
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));
    stop.store(true);
    emitter.join();
    for (auto &t: churn) {
        t.join();
    }

    REQUIRE(dispatches.load() > 0);
    bus.Reset();
}
