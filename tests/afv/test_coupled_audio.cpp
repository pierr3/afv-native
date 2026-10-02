#include "afv-native/afv/ATCRadioSimulation.h"
#include "afv-native/event/EventBus.h"
#include <algorithm>
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <future>
#include <memory>
#include <opus.h>
#include <vector>

using namespace afv_native;
using namespace afv_native::afv;

namespace {
    constexpr unsigned baseFrequency = 118000000;
    constexpr unsigned frequencyStep = 25000;

    class EffectSamples: public audio::ISampleStorage {
        mutable std::array<float, audio::frameSizeSamples * 2> samples;

      public:
        EffectSamples() {
            for (size_t i = 0; i < samples.size(); ++i) {
                samples[i] = 0.02f * std::sin(i * 0.13f);
            }
        }
        float *data() const override {
            return samples.data();
        }
        size_t lengthInSamples() const override {
            return samples.size();
        }
    };

    std::shared_ptr<ATCRadioSimulation> makeSimulation(unsigned count, bool headset = true, bool effects = false, bool agc = false) {
        auto resources            = std::make_shared<EffectResources>("");
        resources->mClick         = std::make_shared<EffectSamples>();
        resources->mCrackle       = std::make_shared<EffectSamples>();
        resources->mAcBus         = std::make_shared<EffectSamples>();
        resources->mVhfWhiteNoise = std::make_shared<EffectSamples>();
        resources->mHfWhiteNoise  = std::make_shared<EffectSamples>();
        auto sim = std::make_shared<ATCRadioSimulation>(resources, nullptr);
        sim->setupDevices();
        sim->setEnableOutputEffects(effects);
        sim->setEnableAgc(agc);
        for (unsigned i = 0; i < count; ++i) {
            sim->addFrequency(baseFrequency + i * frequencyStep, headset);
            sim->setTx(baseFrequency + i * frequencyStep, true);
            sim->setCrossCoupleAcross(baseFrequency + i * frequencyStep, true);
        }
        return sim;
    }

    // Feed the same real Opus packets through each simulation's public receive path.
    // Each packet describes a single caller heard on four coupled frequencies.
    class Transmission {
        std::unique_ptr<OpusEncoder, decltype(&opus_encoder_destroy)> encoder {nullptr, opus_encoder_destroy};
        unsigned sequence = 0;
        double   toneHz;

      public:
        explicit Transmission(double tone = 440.0): toneHz(tone) {
            int status;
            encoder.reset(opus_encoder_create(audio::sampleRateHz, 1, OPUS_APPLICATION_VOIP, &status));
            REQUIRE(status == OPUS_OK);
        }

        dto::AudioRxOnTransceivers packet(const std::string &callsign = "TEST_PILOT") {
            std::array<float, audio::frameSizeSamples> samples;
            for (size_t i = 0; i < samples.size(); ++i) {
                samples[i] = 0.1f * std::sin(2.0 * 3.141592653589793 * toneHz *
                                             (sequence * samples.size() + i) / audio::sampleRateHz);
            }
            dto::AudioRxOnTransceivers pkt;
            pkt.Callsign        = callsign;
            pkt.SequenceCounter = sequence++;
            pkt.LastPacket      = false;
            pkt.Audio.resize(4000);
            const int size = opus_encode_float(encoder.get(), samples.data(),
                                               samples.size(), pkt.Audio.data(),
                                               pkt.Audio.size());
            REQUIRE(size > 0);
            pkt.Audio.resize(size);
            for (unsigned i = 0; i < 4; ++i) {
                dto::RxTransceiver rx;
                rx.ID            = i;
                rx.Frequency     = baseFrequency + i * frequencyStep;
                rx.DistanceRatio = 1.0f;
                pkt.Transceivers.push_back(rx);
            }
            return pkt;
        }
    };

    std::vector<float> render(ATCRadioSimulation &sim, bool headset = true) {
        std::vector<float> out(audio::frameSizeSamples * (headset ? 2 : 1));
        REQUIRE(sim.getAudioFrame(out.data(), headset) == audio::SourceStatus::OK);
        return out;
    }

    void requireSameAudio(const std::vector<float> &actual, const std::vector<float> &expected) {
        REQUIRE(actual.size() == expected.size());
        float maxError = 0;
        for (size_t i = 0; i < actual.size(); ++i) {
            maxError = std::max(maxError, std::abs(actual[i] - expected[i]));
        }
        REQUIRE(maxError < 1e-6f);
    }
} // namespace

TEST_CASE("coupled frequencies do not amplify a caller", "[radiosim][coupled]") {
    bool headset = true;
    bool effects = false;
    bool agc     = false;
    SECTION("headset") {
    }
    SECTION("speaker") {
        headset = false;
    }
    SECTION("radio effects") {
        effects = true;
    }
    SECTION("normaliser") {
        agc = true;
    }
    auto         single  = makeSimulation(1, headset, effects, agc);
    auto         coupled = makeSimulation(4, headset, effects, agc);
    Transmission tx;
    float        peak = 0;
    for (int frame = 0; frame < 12; ++frame) {
        const auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        coupled->rxVoicePacket(pkt);
        const auto reference = render(*single, headset);
        for (float sample: reference) {
            peak = std::max(peak, std::abs(sample));
        }
        requireSameAudio(render(*coupled, headset), reference);
    }
    REQUIRE(peak > 0.001f);
    for (unsigned i = 0; i < 4; ++i) {
        REQUIRE(coupled->getRxActive(baseFrequency + i * frequencyStep));
    }
}

TEST_CASE("inaudible radios cannot suppress another reception path", "[radiosim][coupled]") {
    auto           single  = makeSimulation(1);
    auto           coupled = makeSimulation(4);
    const unsigned muted   = baseFrequency;
    SECTION("muted") {
        coupled->setOutputMute(muted, true);
    }
    SECTION("zero gain") {
        coupled->setGain(muted, 0);
    }
    SECTION("RX disabled") {
        coupled->setRx(muted, false);
    }
    SECTION("transmitting") {
        coupled->setPtt(true);
        for (unsigned i = 1; i < 4; ++i) {
            coupled->setTx(baseFrequency + i * frequencyStep, false);
        }
    }
    SECTION("removed") {
        coupled->removeFrequency(muted);
    }
    Transmission tx;
    for (int frame = 0; frame < 8; ++frame) {
        auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        coupled->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
    }
}

TEST_CASE("coupled audio selects the strongest available reception", "[radiosim][coupled]") {
    auto         single  = makeSimulation(1, true, true);
    auto         coupled = makeSimulation(4, true, true);
    Transmission tx;
    for (int frame = 0; frame < 8; ++frame) {
        auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        for (unsigned i = 0; i < 3; ++i) {
            pkt.Transceivers[i].DistanceRatio = 0.1f;
        }
        if (frame % 2) {
            std::reverse(pkt.Transceivers.begin(), pkt.Transceivers.end());
        }
        coupled->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
    }
}

TEST_CASE("equal reception respects the highest audible radio gain", "[radiosim][coupled]") {
    auto single  = makeSimulation(1);
    auto coupled = makeSimulation(4);
    single->setGainAll(0.7f);
    coupled->setGainAll(0.2f);
    coupled->setGain(baseFrequency + 2 * frequencyStep, 0.7f);
    Transmission tx;
    for (int frame = 0; frame < 8; ++frame) {
        auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        coupled->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
    }
}

TEST_CASE("deduplication is independent for left right and speaker outputs", "[radiosim][coupled]") {
    auto single  = makeSimulation(1);
    auto coupled = makeSimulation(4);
    coupled->setPlaybackChannel(baseFrequency, PlaybackChannel::Left);
    coupled->setPlaybackChannel(baseFrequency + frequencyStep, PlaybackChannel::Both);
    coupled->setPlaybackChannel(baseFrequency + 2 * frequencyStep, PlaybackChannel::Right);
    coupled->setOnHeadset(baseFrequency + 3 * frequencyStep, false);
    auto         speaker = makeSimulation(1, false);
    Transmission tx;
    for (int frame = 0; frame < 8; ++frame) {
        const auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        coupled->rxVoicePacket(pkt);
        speaker->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
        requireSameAudio(render(*coupled, false), render(*speaker, false));
    }
}

TEST_CASE("different callers remain audible on their own stereo channels", "[radiosim][coupled]") {
    auto left    = makeSimulation(1, true, true);
    auto right   = makeSimulation(1, true, true);
    auto coupled = makeSimulation(4, true, true);
    coupled->setPlaybackChannel(baseFrequency, PlaybackChannel::Left);
    coupled->setPlaybackChannel(baseFrequency + frequencyStep, PlaybackChannel::Right);
    coupled->setPlaybackChannel(baseFrequency + 2 * frequencyStep, PlaybackChannel::Both);
    coupled->setPlaybackChannel(baseFrequency + 3 * frequencyStep, PlaybackChannel::Both);
    Transmission first;
    Transmission second(660.0);
    for (int frame = 0; frame < 8; ++frame) {
        auto a = first.packet("FIRST");
        auto b = second.packet("SECOND");
        left->rxVoicePacket(a);
        right->rxVoicePacket(b);
        // A is heard only on the left-hand radio; B is heard on the right-hand
        // radio and both copies routed to Both. Each ear has a different mix.
        a.Transceivers.resize(1);
        b.Transceivers.erase(b.Transceivers.begin());
        coupled->rxVoicePacket(a);
        coupled->rxVoicePacket(b);
        const auto l        = render(*left);
        const auto r        = render(*right);
        auto       expected = r;
        for (size_t i = 0; i < expected.size(); i += 2) {
            expected[i] += l[i];
        }
        requireSameAudio(render(*coupled), expected);
    }
}

TEST_CASE("distinct callers and real frequency blocking are preserved", "[radiosim][coupled]") {
    auto         single  = makeSimulation(1);
    auto         coupled = makeSimulation(4);
    Transmission first;
    Transmission second(660.0);
    for (int frame = 0; frame < 8; ++frame) {
        auto a = first.packet("FIRST");
        auto b = second.packet("SECOND");
        single->rxVoicePacket(a);
        single->rxVoicePacket(b);
        coupled->rxVoicePacket(a);
        coupled->rxVoicePacket(b);
        requireSameAudio(render(*coupled), render(*single));
    }
    const auto radios = coupled->getRadioState();
    for (const auto &[frequency, radio]: radios) {
        REQUIRE(radio.mLastRxCount == 2);
        REQUIRE(radio.liveTransmittingCallsigns.size() == 2);
    }
    REQUIRE(radios.at(baseFrequency).audioState[0].BlockTone != nullptr);
}

TEST_CASE("routing can change during a transmission without losing the caller", "[radiosim][coupled]") {
    auto         single  = makeSimulation(1);
    auto         coupled = makeSimulation(4);
    Transmission tx;
    for (int frame = 0; frame < 16; ++frame) {
        if (frame == 4) {
            coupled->setOutputMute(baseFrequency, true);
        }
        if (frame == 8) {
            coupled->setRx(baseFrequency + frequencyStep, false);
        }
        if (frame == 12) {
            coupled->removeFrequency(baseFrequency + 2 * frequencyStep);
        }
        const auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        coupled->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
    }
}

TEST_CASE("multiple transceivers of one frequency do not duplicate audio or callsigns", "[radiosim][coupled]") {
    auto         single  = makeSimulation(1);
    auto         coupled = makeSimulation(4);
    Transmission tx;
    for (int frame = 0; frame < 8; ++frame) {
        auto pkt = tx.packet();
        single->rxVoicePacket(pkt);
        const auto copy = pkt.Transceivers;
        pkt.Transceivers.insert(pkt.Transceivers.end(), copy.begin(), copy.end());
        coupled->rxVoicePacket(pkt);
        requireSameAudio(render(*coupled), render(*single));
        for (const auto &[frequency, radio]: coupled->getRadioState()) {
            REQUIRE(radio.liveTransmittingCallsigns == std::vector<std::string> {"TEST_PILOT"});
        }
    }
}

TEST_CASE("ending a coupled transmission produces only one squelch click per channel", "[radiosim][coupled]") {
    auto         single  = makeSimulation(1, true, true);
    auto         coupled = makeSimulation(4, true, true);
    Transmission tx;
    for (int frame = 0; frame < 40; ++frame) {
        if (frame < 8) {
            auto pkt       = tx.packet();
            pkt.LastPacket = frame == 7;
            single->rxVoicePacket(pkt);
            coupled->rxVoicePacket(pkt);
        }
        requireSameAudio(render(*coupled), render(*single));
    }
    for (unsigned i = 0; i < 4; ++i) {
        REQUIRE_FALSE(coupled->getRxActive(baseFrequency + i * frequencyStep));
        REQUIRE(coupled->getRadioState()
                    .at(baseFrequency + i * frequencyStep)
                    .liveTransmittingCallsigns.empty());
    }
}

TEST_CASE("disabling all matching receivers silences buffered audio", "[radiosim][coupled]") {
    auto         coupled = makeSimulation(4);
    Transmission tx;
    coupled->rxVoicePacket(tx.packet());
    render(*coupled);
    for (unsigned i = 0; i < 4; ++i) {
        coupled->setRx(baseFrequency + i * frequencyStep, false);
    }
    requireSameAudio(render(*coupled), std::vector<float>(audio::frameSizeSamples * 2, 0.0f));
}

namespace {
    struct PlaybackEventsDrained {
        std::shared_ptr<std::promise<void>> done;
    };

    void drainPlaybackEvents() {
        auto &bus    = event::EventBus::Instance();
        auto  done   = std::make_shared<std::promise<void>>();
        auto  future = done->get_future();
        const auto handler = bus.AddHandler<PlaybackEventsDrained>([](const PlaybackEventsDrained &e) {
            e.done->set_value();
        });
        bus.OnEventAsync(PlaybackEventsDrained {done});
        const auto status = future.wait_for(std::chrono::seconds(2));
        bus.RemoveHandler(handler);
        REQUIRE(status == std::future_status::ready);
    }
} // namespace

TEST_CASE("transmitting radios do not emit a frequency RX begin event", "[radiosim][coupled]") {
    bool transmitting = true;
    SECTION("transmitting") {
    }
    SECTION("receiving") {
        transmitting = false;
    }
    auto sim = makeSimulation(1);
    sim->setPtt(transmitting);
    Transmission tx;
    drainPlaybackEvents();
    auto  hits = std::make_shared<std::atomic<unsigned>>(0);
    auto &bus  = event::EventBus::Instance();
    const auto handler = bus.AddHandler<FrequencyRxBeginEvent>([hits](const FrequencyRxBeginEvent &e) {
        if (e.frequency == baseFrequency) {
            ++*hits;
        }
    });
    sim->rxVoicePacket(tx.packet());
    const auto audio = render(*sim);
    drainPlaybackEvents();
    bus.RemoveHandler(handler);
    REQUIRE(hits->load() == (transmitting ? 0u : 1u));
    if (transmitting) {
        requireSameAudio(audio, std::vector<float>(audio.size(), 0.0f));
    }
}
