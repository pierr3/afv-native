#ifndef AFV_NATIVE_COMMON_TYPES_H
#define AFV_NATIVE_COMMON_TYPES_H

#include "hardwareType.h"
#include <cstddef>
#include <optional>
#include <string>

namespace afv_native {
    struct SimpleAtcRadioState {
        bool            tx;
        bool            rx;
        bool            xc;
        bool            crossCoupleAcross;
        bool            onHeadset;
        unsigned int    Frequency;
        std::string     stationName       = "";
        HardwareType    simulatedHardware = HardwareType::Schmid_ED_137B;
        bool            isATIS            = false;
        PlaybackChannel playbackChannel   = PlaybackChannel::Both;

        std::string lastTransmitCallsign = "";
    };
    struct SimpleAtcStation {
        std::string  name;
        unsigned int frequency;
        unsigned int frequencyAlias;
        // Zero-based position returned by the AFV VCCS endpoint, when available.
        std::optional<std::size_t> vccsOrder = std::nullopt;
    };
} // namespace afv_native

#endif // AFV_NATIVE_COMMON_TYPES_H
