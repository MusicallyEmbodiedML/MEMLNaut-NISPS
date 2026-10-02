#ifndef __VUMETER_HPP__
#define __VUMETER_HPP__

#include <cstddef>
#include <cmath>
#include "src/memllib/audio/AudioDriver.hpp"  // kBufferSize

// VU metering bridge between the audio core and the display.
// processBlock() runs on the audio core from audio_block_callback. It is armed only while the
// VU view is on screen (active == true); otherwise it costs a single branch per audio block.
namespace VUMeter {

    // Channel order: 0 = In L, 1 = In R, 2 = Out L, 3 = Out R.
    inline volatile bool  active = false;            // set by the VU view OnDisplay/OnHide
    inline volatile float levels[4] = {0, 0, 0, 0};  // published envelopes, read by the view
    inline float          env[4]    = {0, 0, 0, 0};  // audio-core-only follower state

    __attribute__((always_inline)) inline void processBlock(
        float in[][kBufferSize], float out[][kBufferSize], size_t n)
    {
        if (!active) return;

        // Simplistic fast-attack / slow-release peak follower, updated once per audio block
        // (~Fs/n ≈ 3 kHz). Coeffs are non-const static (SRAM, not flash) as this is an
        // audio-core hot path — tune on hardware.
        static float kAttack  = 0.6f;    // near-instant rise toward the block peak
        static float kRelease = 0.006f;  // ~50 ms fall

        float pk[4];  // zeroed per element: a {0,...} initialiser compiles to a flash memset
        for (int c = 0; c < 4; ++c) pk[c] = 0.f;
        for (size_t i = 0; i < n; ++i) {
            pk[0] = fmaxf(pk[0], fabsf(in[0][i]));
            pk[1] = fmaxf(pk[1], fabsf(in[1][i]));
            pk[2] = fmaxf(pk[2], fabsf(out[0][i]));
            pk[3] = fmaxf(pk[3], fabsf(out[1][i]));
        }
        for (int c = 0; c < 4; ++c) {
            const float coeff = (pk[c] > env[c]) ? kAttack : kRelease;
            env[c]   += coeff * (pk[c] - env[c]);
            levels[c] = env[c];
        }
    }
}

#endif
