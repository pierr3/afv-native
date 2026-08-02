#include "afv-native/afv/ATCRadioSimulation.h"
#include "afv-native/afv/EffectResources.h"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace afv_native;
using namespace afv_native::afv;

// Exercises the surface where the audio callback thread meets the application
// thread. These assert almost nothing on their own - the point is to give
// ThreadSanitizer two threads hammering the same radio state at once.

TEST_CASE("radio state survives concurrent mutation from two threads", "[radiosim]") {
    // No resource files and no UDP channel: nothing here transmits, and the
    // effect sources stay null, which is fine for lock exercise.
    auto                 resources = std::make_shared<EffectResources>("");
    ATCRadioSimulation   sim(resources, nullptr);

    constexpr unsigned kFreqBase = 118000000;
    for (unsigned i = 0; i < 8; ++i) {
        sim.addFrequency(kFreqBase + i * 25000, true, "TEST" + std::to_string(i));
    }

    std::atomic<bool> stop {false};
    std::atomic<long> reads {0};
    std::atomic<long> writes {0};

    // Stand-in for the application thread: flips radio state constantly.
    std::vector<std::thread> mutators;
    for (int t = 0; t < 3; ++t) {
        mutators.emplace_back([&, t] {
            unsigned i = 0;
            while (!stop.load()) {
                const unsigned freq = kFreqBase + (i++ % 8) * 25000;
                sim.setRx(freq, (i & 1) != 0);
                sim.setTx(freq, (i & 2) != 0);
                sim.setXc(freq, (i & 4) != 0);
                sim.setGain(freq, 0.5f);
                sim.setOutputMute(freq, (i & 8) != 0);
                ++writes;
            }
        });
    }

    // Stand-in for the miniaudio real-time callback: pulls mixed frames while
    // the mutators above are reshaping the radio state underneath it.
    std::thread audioThread([&] {
        std::vector<audio::SampleType> buffer(audio::frameSizeSamples);
        while (!stop.load()) {
            (void) sim.getAudioFrame(buffer.data(), true);
            (void) sim.getAudioFrame(buffer.data(), false);
        }
    });

    // Stand-in for anything polling state for a UI.
    std::vector<std::thread> readers;
    for (int t = 0; t < 2; ++t) {
        readers.emplace_back([&] {
            unsigned i = 0;
            while (!stop.load()) {
                const unsigned freq = kFreqBase + (i++ % 8) * 25000;
                (void) sim.getRxState(freq);
                (void) sim.getTxState(freq);
                (void) sim.getTxActive(freq);
                (void) sim.getRadioState();
                (void) sim.makeTransceiverDto();
                ++reads;
            }
        });
    }

    std::this_thread::sleep_for(std::chrono::seconds(3));
    stop.store(true);
    audioThread.join();
    for (auto &t: mutators) {
        t.join();
    }
    for (auto &t: readers) {
        t.join();
    }

    REQUIRE(reads.load() > 0);
    REQUIRE(writes.load() > 0);
}

TEST_CASE("adding and removing frequencies races cleanly against readers", "[radiosim]") {
    auto               resources = std::make_shared<EffectResources>("");
    ATCRadioSimulation sim(resources, nullptr);

    std::atomic<bool> stop {false};
    std::atomic<long> ops {0};

    std::thread churn([&] {
        unsigned i = 0;
        while (!stop.load()) {
            const unsigned freq = 118000000 + (i++ % 16) * 25000;
            sim.addFrequency(freq, true, "CHURN");
            sim.setRx(freq, true);
            sim.removeFrequency(freq);
            ++ops;
        }
    });

    std::thread reader([&] {
        while (!stop.load()) {
            (void) sim.getRadioState();
            (void) sim.makeTransceiverDto();
        }
    });

    std::this_thread::sleep_for(std::chrono::seconds(3));
    stop.store(true);
    churn.join();
    reader.join();

    REQUIRE(ops.load() > 0);
}
