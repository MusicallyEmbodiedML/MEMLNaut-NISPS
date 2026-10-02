/*
memlDaisyPitchShifter — RP2350/SRAM-optimised copy of daisysp::PitchShifter.

Based on daisysp PitchShifter, Copyright (c) 2020 Electrosmith, Corp (shensley).
Use of this source code is governed by an MIT-style license that can be found in
the LICENSE file or at https://opensource.org/licenses/MIT.

Same algorithm (two crossfaded taps swept across a delay line by antiphase phasors),
reworked so the per-sample path never executes from flash:
  - Process() is placed in SRAM (.time_critical); everything it calls is force-inlined
    (no out-of-line Phasor::Process / DelayLine::Read / myrand / sinf calls into XIP flash).
  - sinf crossfade window replaced by a polynomial sin(pi*x) approximation (max err ~0.001).
  - The two daisysp delay lines always received identical input, so they are merged into
    one buffer read by two taps (half the memory and half the writes).
  - Phasors run normalised (0..1) instead of radians; no modulo in the delay indexing.

Transposition is continuous (non-integer semitones): ratio = 2^(t/12). Upward shifts match
daisysp exactly at integer semitones below 24. Downward shifts use the correct sweep rate
(daisysp used ratio 2-r, so e.g. -12 froze the pitch at 0 Hz).

SetTransposition() calls exp2f, so call it at control rate, not per sample.
*/

#pragma once
#ifndef MEML_DAISY_PITCHSHIFTER_H
#define MEML_DAISY_PITCHSHIFTER_H

#include <stdint.h>
#include <stddef.h>
#include <cmath>
#include "pico.h"

#ifndef MEML_PS_BUFFER_SIZE
#define MEML_PS_BUFFER_SIZE 2400  // 50 ms at 48 kHz, as daisysp SHIFT_BUFFER_SIZE
#endif

#define MEML_PS_RAM_FUNC __attribute__((section(".time_critical.memlDaisyPitchShifter")))

class memlDaisyPitchShifter
{
  public:
    static constexpr size_t kBufferSize = MEML_PS_BUFFER_SIZE;

    void Init(float sr)
    {
        sr_ = sr;
        for(size_t i = 0; i < kBufferSize; i++)
            line_[i] = 0.f;
        write_ptr_ = 0;
        phs_[0]    = 0.f;
        phs_[1]    = 0.5f;  // antiphase (daisysp: 0 and PI radians)
        prev_phs_[0] = prev_phs_[1] = 0.f;
        for(size_t n = 0; n < 2; n++)
        {
            mod_amt_[n]    = 0.f;
            slewed_mod_[n] = 0.f;
            mod_coeff_[n]  = 0.f;
        }
        fun_       = 0.f;
        seed_      = 1;
        transpose_ = 0.f;
        SetDelSize(kBufferSize);  // also applies transpose_
    }

    /** Process one sample. Runs from SRAM; all callees are inlined. */
    MEML_PS_RAM_FUNC float Process(float in)
    {
        // Phasors return the pre-increment value (as daisysp Phasor::Process)
        float fade[2];
        for(size_t n = 0; n < 2; n++)
        {
            fade[n] = phs_[n];
            phs_[n] += inc_;
            if(phs_[n] >= 1.f)
                phs_[n] -= 1.f;
        }

        // On each phasor wrap, pick a new random tape-flutter target
        for(size_t n = 0; n < 2; n++)
        {
            if(prev_phs_[n] > fade[n])
            {
                mod_amt_[n]   = fun_ * rand01() * (del_size_f_ * 0.5f);
                mod_coeff_[n] = 0.0002f + rand01() * 0.001f;
            }
            slewed_mod_[n] += mod_coeff_[n] * (mod_amt_[n] - slewed_mod_[n]);
            prev_phs_[n] = fade[n];
        }

        if(shift_up_)
        {
            fade[0] = 1.f - fade[0];
            fade[1] = 1.f - fade[1];
        }

        write(in);

        float val = 0.f;
        for(size_t n = 0; n < 2; n++)
        {
            const float gain = sinPiApprox(fade[n]);
            const float dly  = fade[n] * del_size_m1_ + slewed_mod_[n];
            val += read(dly) * gain;
        }
        return val;
    }

    /** Transposition in semitones; non-integer values allowed. Control rate (uses exp2f). */
    void SetTransposition(float transpose)
    {
        transpose_ = transpose;
        shift_up_  = transpose > 0.f;
        const float ratio = exp2f(transpose * (1.f / 12.f));
        // Tap sweep rate that yields `ratio`: |ratio - 1| = f * del_size / sr
        const float mod_freq = fabsf(ratio - 1.f) * sr_ / del_size_f_;
        inc_ = mod_freq / sr_;
    }

    /** Delay size in samples (changes the timbre of the shifting). */
    void SetDelSize(uint32_t size)
    {
        del_size_    = size < kBufferSize ? size : kBufferSize;
        del_size_f_  = static_cast<float>(del_size_);
        del_size_m1_ = del_size_f_ - 1.f;
        SetTransposition(transpose_);
    }

    /** Amount of internal random modulation, sounds a bit like tape flutter. */
    inline void SetFun(float f) { fun_ = f; }

  private:
    // daisysp DelayLine semantics: write pointer runs backwards, read at write_ptr + delay
    // after the write (so delay 1 = the sample just written).
    __force_inline void write(float x)
    {
        line_[write_ptr_] = x;
        write_ptr_        = (write_ptr_ == 0) ? kBufferSize - 1 : write_ptr_ - 1;
    }

    __force_inline float read(float delay) const
    {
        int32_t d = static_cast<int32_t>(delay);
        if(d < 0)
            d = 0;
        const float frac = delay - static_cast<float>(d);
        if(static_cast<size_t>(d) >= kBufferSize)
            d = kBufferSize - 1;
        size_t i0 = write_ptr_ + static_cast<size_t>(d);
        if(i0 >= kBufferSize)
            i0 -= kBufferSize;
        size_t i1 = i0 + 1;
        if(i1 >= kBufferSize)
            i1 -= kBufferSize;
        const float a = line_[i0];
        const float b = line_[i1];
        return a + (b - a) * frac;
    }

    // sin(pi*x) for x in [0,1]: parabola 4x(1-x) plus a quadratic correction (max err ~0.001)
    static __force_inline float sinPiApprox(float x)
    {
        const float y = 4.f * x * (1.f - x);
        return y * (0.775f + 0.225f * y);
    }

    // xorshift32 (as daisysp hash_xs32), mapped to [0,1] like (rand() % 255) / 255
    __force_inline float rand01()
    {
        uint32_t x = seed_;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        seed_ = x;
        return static_cast<float>(x % 255u) * (1.f / 255.f);
    }

    float    line_[kBufferSize];
    size_t   write_ptr_;
    uint32_t del_size_;
    float    del_size_f_, del_size_m1_;
    float    sr_;
    bool     shift_up_;
    float    phs_[2], prev_phs_[2], inc_;
    float    transpose_;
    float    fun_;
    float    mod_amt_[2], slewed_mod_[2], mod_coeff_[2];
    uint32_t seed_;
};

#endif
