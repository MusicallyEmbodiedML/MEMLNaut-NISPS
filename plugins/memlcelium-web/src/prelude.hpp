// Force-included ahead of everything (-include). Stands in for the firmware's
// AudioDriver.hpp and InterfaceBase.hpp, which pull in the I2S driver, MIDI and the
// Pico SDK: defining their include guards here keeps the real headers out, and the
// few names the audio app needs from them are declared below instead.
#pragma once

#define __AUDIO_DRIVER_HPP__
#define __INTERFACE_BASE_HPP__

#include <cstddef>
#include <cmath>
#include <WString.h>
#include "pico.h"
#include "pico/util/queue.h"

struct stereosample_t {
    float L, R;
};

using audiocallback_fptr_t = stereosample_t (*)(stereosample_t);

class AudioDriver {
public:
    typedef struct {
        bool mic_input;
        size_t line_level;
        size_t mic_gain_dB;
        float output_volume;
    } codec_config_t;
    static inline void SetCallback(audiocallback_fptr_t) {}
};

// The audio app only asks its interface for new parameters; the browser glue hands
// them over directly through ProcessParams() instead, so this always says "none".
class InterfaceBase {
public:
    virtual ~InterfaceBase() = default;
    bool ReceiveParamsFromQueue(float*) { return false; }
};

// From PicoDefs.hpp (identical definitions, so a later include of it is harmless).
#define DEBUG_PRINT(...)
#define DEBUG_PRINTLN(...)
#define DEBUG_PRINTF(...)

// Arduino's global min()/max(), which maxiPAF.hpp calls unqualified.
template <typename A, typename B> constexpr auto min(A a, B b) { return a < b ? a : b; }
template <typename A, typename B> constexpr auto max(A a, B b) { return a > b ? a : b; }
