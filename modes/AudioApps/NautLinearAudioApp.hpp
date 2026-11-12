#ifndef __NAUTLINEAR_AUDIO_APP_HPP__
#define __NAUTLINEAR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <cmath>
#include <vector>

#include "../../src/memllib/synth/OnePoleSmoother.hpp"
#include "../../src/memllib/synth/Oversampler.hpp"
#include "../../src/memllib/synth/maximilian.h"

// ---------------------------------------------------------------------------
// NautLinear — NN-driven parallel waveshaper bank.
//
// Signal chain (mono):
//
//   in L+R -> auto-level -> [4x oversampled: preamp -> 8 shapers in parallel
//   -> weighted mix -> damper (slew limit, saturation)] -> DC block
//   -> auto makeup gain (wet matched to dry loudness) -> wet/dry (RVY1)
//
// There is no tone filtering: tone comes from the nonlinearities. (The only
// filters are the DC blocker and the oversampler's anti-aliasing filters.)
//
// Preamp: drive into tanh with a bias for asymmetry (even harmonics). The bias
// sags with the input envelope (tube-style bias shift): loud playing moves the
// operating point, so attack and sustain get different harmonics. The static
// offset tanh(bias) is subtracted, so bias changes don't thump.
//
// Damper: at the end of the wet path, so inside the feedback loop. A slew-rate
// limiter (nonlinear: bites only on fast, loud content, so it tames screaming
// high-frequency feedback) and a soft saturator (bounds the loop level).
//
// Feedback: the wet mix is fed back one sample late, scaled by a master
// feedback gain and then by a separate bipolar gain per shaper, into each
// shaper's input. Inside the oversampler it is linearly interpolated across the
// sub-samples. The tap point is a per-model option (fbPreMakeup_):
//   pre-makeup  (default): after the damper, before makeup gain and the
//                mix fade, clamped to ±1. The loop can sustain and
//                self-oscillate without input.
//   post-makeup: the final wet signal (after makeup, fade and limiter). Makeup
//                follows the dry input, so the loop mostly reshapes tone and
//                dies away with the input.
// Makeup has a -12 dB floor so a self-oscillating loop stays audible when the
// dry input is silent.
//
// Shapers: hard clip, tanh clip, variable-knee clip and diode clip share a
// master drive (each with its own trim); the three symmetric clippers also get
// a shared bias for even harmonics. Octave fuzz (half -> full-wave rectifier
// into tanh), asymmetric power shaper, sine folder and Chebyshev harmonic mixer
// complete the bank. Sine and Chebyshev have their own input levels, because
// they depend on input amplitude rather than drive.
//
// CPU: the whole bank shares ONE oversampler. The mix is linear, so
// decimating the weighted sum equals summing individually decimated shapers,
// at 1/8 of the filter cost. The shapes are the memllib shaper curves
// (HardClip, TanhSoftClip, VariableKneeClip, DiodeClip, AsymPowerShaper,
// SineShaper, ChebyshevShaper) computed inline with fast tanh/exp2/log2/sin
// approximations — libm calls at 4x rate for 8 shapers overran the audio
// block ~6x. All parameter mapping and makeup gain run at control rate (every
// kCtrlInterval samples).
//
// Table shaper (ninth, built only with NAUTLINEAR_TABLE=1): a 4D wavetable of
// transfer curves, 192 KB in SRAM:
//   x (1024) : the audio input
//   y (8)    : input level (the preamp's sag envelope), so the curve changes with
//              how hard you play: near-linear when quiet, rich when loud
//   z (4)    : harmonic centre, around the 2nd .. 12th harmonic (NN param)
//   w (3)    : odd only / balanced / even-weighted (NN param)
// Each curve is built at boot from a harmonic recipe as a Chebyshev sum (the
// fundamental plus harmonics 2..16), so a full-scale sine comes out with exactly
// that recipe. z and w move at control rate, so the (z, w) blend is baked into an
// 8 x 1024 float slice, rebuilt a few points per sample while they move; the audio
// path only does bilinear (y, x) reads from the slice. Off by default: it costs
// ~70 us per block, which left too little headroom with machine listening.
// With it off, the params, enables, focus groups and save format are the
// 8-shaper layout, and the table's RAM is not allocated.
// ---------------------------------------------------------------------------

#ifndef NAUTLINEAR_TABLE
#define NAUTLINEAR_TABLE 0
#endif

// Param map (NN outputs, 0..1). Sections match the RL screen group labels.
namespace NautLinearParams {
    constexpr size_t kChebyHarmonics = 6;  // T2..T7
    constexpr size_t kNShapers = NAUTLINEAR_TABLE ? 9 : 8;
    enum : size_t {
        // --- Pre ---
        AUTOLEVEL = 0,   // 0 = fixed input gain .. 1 = fully level-normalised input
        PRE_DRIVE,       // preamp drive, 0 .. +30 dB
        PRE_ASYM,        // preamp bias, -1 .. +1 (0.5 = symmetric)
        PRE_SAG,         // bias shift with the input envelope, 0 .. 1
        PRE_SAG_REC,     // sag recovery time, 20 ms .. 1 s (exp)
        DRIVE,           // master drive, 0 .. +30 dB (hard/tanh/knee/diode/octave)
        BIAS,            // bias into hard/tanh/knee, -0.4 .. +0.4 (0.5 = none)
        // --- Shape ---
        HARD_TRIM,       // per-shaper drive trims, -12 .. +12 dB (0.5 = master)
        TANH_TRIM,
        KNEE_TRIM,
        KNEE_HARDNESS,   // 0 soft .. 1 hard
        DIODE_TRIM,
        DIODE_ETA_POS,   // diode softness per half, 0.05 .. 2
        DIODE_ETA_NEG,
        OCT_TRIM,
        OCT_RECTIFY,     // 0 half-wave .. 1 full-wave (octave up)
        ASYM_EXP_POS,    // asymmetric shaper exponents, -3 .. 3
        ASYM_EXP_NEG,
        SINE_IN,         // sine folder input level, 0.1 .. 2 (exp)
        SINE_GAIN,
        SINE_ASYM,
        CHEBY_IN,        // Chebyshev input level, 0.25 .. 2 (exp)
        CHEBY_T2,        // Chebyshev harmonic gains T2..T7, bipolar
        // CHEBY_T3 .. CHEBY_T7 follow
#if NAUTLINEAR_TABLE
        TABLE_IN = CHEBY_T2 + kChebyHarmonics,  // table shaper input level, 0.25 .. 2 (exp)
        TABLE_HARM,      // table harmonic centre (z), 0 .. 1
        TABLE_OE,        // table odd/even balance (w), 0 = odd only .. 1 = even-weighted
        // --- Mix --- (order: hard, tanh, knee, diode, octave, asym, sine, cheby[, table])
        MIX,
#else
        // --- Mix --- (order: hard, tanh, knee, diode, octave, asym, sine, cheby)
        MIX = CHEBY_T2 + kChebyHarmonics,
#endif
        // --- Damp ---
        DAMP_SLEW = MIX + kNShapers,  // slew limit, 0 = off .. 1 = heavy (exp)
        DAMP_SAT,               // end saturation drive, 0 .. +18 dB
        // --- FB ---
        FB_MASTER,              // master feedback gain, 0 .. 2 (squared curve)
        FB_SHAPER,              // per-shaper feedback gains, -1 .. 1, same order as MIX
        COUNT = FB_SHAPER + kNShapers
    };
}

// Fast approximations for the oversampled shaper bank.
namespace NautFast {
    // tanh: Padé-style rational, exact ±1 at |x| = 3 and continuous there.
    static __force_inline float tanh(float x) {
        if (x > 3.f)  return 1.f;
        if (x < -3.f) return -1.f;
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }

    // 2^x, clamped to x in [-60, 60]; cubic on the fraction (~1e-4 rel error).
    static __force_inline float exp2(float x) {
        if (x < -60.f) x = -60.f;
        else if (x > 60.f) x = 60.f;
        int32_t i = static_cast<int32_t>(x);
        if (x < static_cast<float>(i)) --i;  // floor
        const float f = x - static_cast<float>(i);
        const float p = 1.f + f * (0.6951786f + f * (0.2261475f + f * 0.0781794f));
        int32_t bits;
        std::memcpy(&bits, &p, sizeof(bits));
        bits += i << 23;
        float out;
        std::memcpy(&out, &bits, sizeof(out));
        return out;
    }

    // log2 for positive normal x (~2e-4 abs error).
    static __force_inline float log2(float x) {
        int32_t bits;
        std::memcpy(&bits, &x, sizeof(bits));
        const float e = static_cast<float>(((bits >> 23) & 0xff) - 127);
        bits = (bits & 0x007fffff) | 0x3f800000;  // mantissa in [1, 2)
        float m;
        std::memcpy(&m, &bits, sizeof(m));
        // quartic fit of log2 on [1, 2)
        return e + (-2.49684606f + m * (4.02854794f + m * (-2.08121416f + m * (0.62887362f - 0.07915816f * m))));
    }

    // sin(2*pi*t), t in turns. Parabolic approximation with one correction step (~1e-3).
    static __force_inline float sinTurns(float t) {
        const float u = t + 0.5f;
        int32_t i = static_cast<int32_t>(u);
        if (u < static_cast<float>(i)) --i;  // floor
        const float r = t - static_cast<float>(i);  // [-0.5, 0.5)
        const float ar = r < 0.f ? -r : r;
        float y = 8.f * r - 16.f * r * ar;
        const float ay = y < 0.f ? -y : y;
        y = 0.225f * (y * ay - y) + y;
        return y;
    }
}

template<size_t NPARAMS = NautLinearParams::COUNT>
class NautLinearAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t kCtrlInterval = 64;  // samples per control-rate update (1.3 ms)

    queue_t wetdryQueue;

    // Feedback tap point (Options screen, saved per model): true = before makeup gain.
    volatile bool fbPreMakeup_{true};

    // Enable screen (saved per model). Bits 0..kNShapers-1 = shapers in MIX order
    // (hard, tanh, knee, diode, octave, asym, sine, cheby[, table]), then feedback,
    // preamp and damper. Disabled blocks are skipped, not just muted (saves CPU).
    static constexpr size_t   kNShapers   = NautLinearParams::kNShapers;
    static constexpr uint32_t kEnTable    = 1u << 8;   // only with NAUTLINEAR_TABLE
    static constexpr uint32_t kEnFeedback = 1u << kNShapers;
    static constexpr uint32_t kEnPre      = 1u << (kNShapers + 1);
    static constexpr uint32_t kEnDamp     = 1u << (kNShapers + 2);
    static constexpr uint32_t kNEnables   = kNShapers + 3;
    static constexpr uint32_t kEnableAll  = (1u << kNEnables) - 1u;

#if NAUTLINEAR_TABLE
    // Table shaper dimensions (see the header comment).
    static constexpr size_t kTabX = 1024, kTabY = 8, kTabZ = 4, kTabW = 3;
    static constexpr size_t kTabHarm = 16;               // highest harmonic in a recipe
    static constexpr size_t kSliceSize = kTabY * kTabX;  // 8192, a power of 2
    static constexpr size_t kSlicePointsPerSample = 8;   // full rebuild in 1024 samples
#endif
    volatile uint32_t enableMask_{kEnableAll};

    void setWetDryQueued(float value) {
        queue_try_add(&wetdryQueue, &value);
    }

    NautLinearAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&wetdryQueue, sizeof(float), 1);
    };

    __attribute__((hot)) stereosample_t __force_inline Process(const stereosample_t x) override
    {
        if (ctrlCount_ == 0) updateControl();
        if (++ctrlCount_ >= kCtrlInterval) ctrlCount_ = 0;

        // mono sum of the input — all processing is mono.
        const float mono = x.L + x.R;

        // input loudness: slow (auto-level) and fast (makeup reference) mean-abs envelopes.
        const float inAbs = fabsf(mono);
        inEnvSlow_ += envSlowCoef_ * (inAbs - inEnvSlow_);
        dryEnv_    += envFastCoef_ * (inAbs - dryEnv_);

        // auto-level, then the oversampled preamp + shaper bank + damper.
        const float s = mono * preGain_;

        // preamp sag: envelope of the levelled input (fast attack, NN-set recovery)
        // shifts the preamp bias; constant across this sample's sub-samples.
        const float sAbs = fabsf(s);
        sagEnv_ += ((sAbs > sagEnv_) ? sagAtkCoef_ : sagRelCoef_) * (sAbs - sagEnv_);
        preBiasNow_ = preBias_ - preSagK_ * sagEnv_;
        preBiasOff_ = NautFast::tanh(preBiasNow_);

#if NAUTLINEAR_TABLE
        // table shaper: level row (y) from the same envelope, on a concave curve
        // ly*(2-ly) for resolution at low levels (no sqrtf: it's a library call);
        // finish any slice rebuild a few points at a time.
        if (enMask_ & kEnTable) {
            float ly = sagEnv_ * tabYScale_;
            if (ly > 1.f) ly = 1.f;
            const float ty = ly * (2.f - ly) * tabYMax_;
            size_t y0 = static_cast<size_t>(ty);
            if (y0 > kTabY - 2) y0 = kTabY - 2;
            tabFy_ = ty - static_cast<float>(y0);
            tabRow0_ = &slice_[y0][0];
            tabRow1_ = &slice_[y0 + 1][0];
            if (sliceSweep_ > 0) rebuildSlice();
        }
#endif
        // feedback ramps from where it is to last sample's wet over the 4 sub-samples.
        fbStep_ = (fbLast_ - fbInterp_) * fbStepScale_;
        const float mixed = bankOS_.process(s, [this](float v) __attribute__((always_inline)) {
            return shapeAll(v);
        });

        // mixed is the unit-weight blend after the damper (the overall level is
        // applied by makeup * fade below); remove DC (bias, rectifier, even harmonics).
        const float blend = wetDC_.play(mixed, dcCoef_);

        // makeup gain matches the wet loudness to the dry input, so param changes
        // are heard as tone rather than level; fade_ = min(sum of mix levels, 1)
        // lets the net still pull the wet signal down.
        wetEnv_ += envFastCoef_ * (fabsf(blend) - wetEnv_);
        // soft output limiter (unity gain for small signals, peaks under ±0.84): makeup
        // can push a peaky wet signal past ±1, and a hard limit there clipped audibly.
        const float wet = NautFast::tanh(blend * makeup_ * fade_ * outLimDrive_) * outLimNorm_;

        // feedback tap. Pre-makeup is clamped for safety (the DC blocker can
        // overshoot slightly); post-makeup is already limited.
        float fbTap = fbPreMakeup_ ? blend : wet;
        if (fbTap > wetLimit_) fbTap = wetLimit_;
        else if (fbTap < -wetLimit_) fbTap = -wetLimit_;
        fbLast_ = (enMask_ & kEnFeedback) ? fbTap : 0.f;

        const float y = (wet * wetG_) + (mono * dryG_);

        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRate_ = sample_rate;

        envFastCoef_ = 1.f - expf(-1.f / (0.08f * sample_rate));   // 80 ms
        envSlowCoef_ = 1.f - expf(-1.f / (0.3f * sample_rate));    // 300 ms
        const float ctrlRate = sample_rate / static_cast<float>(kCtrlInterval);
        makeupCoef_ = 1.f - expf(-1.f / (0.05f * ctrlRate));       // 50 ms
        smoother.Setup(150.f, ctrlRate);  // smoothing runs at control rate
        sagAtkCoef_ = 1.f - expf(-1.f / (0.001f * sample_rate));  // 1 ms attack

#if NAUTLINEAR_TABLE
        buildTable();
        tabZ_ = tabW_ = -1.f;                     // force a slice for the first params
        setTableCorners(0.f, 0.f);
        sliceSweep_ = 0;
        for (size_t i = 0; i < kSliceSize / kSlicePointsPerSample; ++i) rebuildSlice();
#endif
    }

    __attribute__((always_inline)) void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        {
            float v;
            if (queue_try_remove(&wetdryQueue, &v)) wetdryKnobValue = v;
        }
        if (wetdryKnobValue >= 0.f) {
            wetdry_mix_ = wetdryKnobValue;
        }
        neuralNetOutputs = params;
    }

protected:

    // All shapers on one oversampled sample, returned as the weighted mix.
    // Every per-shaper constant is precomputed in updateControl().
    __force_inline float shapeAll(float v) {
        // feedback, interpolated across the oversampled sub-samples
        fbInterp_ += fbStep_;
        const float fb = fbInterp_;

        const uint32_t en = enMask_;

        // preamp: tanh(drive*x + bias) - tanh(bias), bias sagging with the envelope
        if (en & kEnPre) v = NautFast::tanh(v * preDrive_ + preBiasNow_) - preBiasOff_;

        const float vb = v + bias_;  // biased feed for the symmetric clippers
        float acc = 0.f;

        // hard clip (HardClip, ceiling 1)
        if (en & (1u << 0)) {
            float hard = (vb + fbG_[0] * fb) * hardDrive_;
            if (hard > 1.f) hard = 1.f;
            else if (hard < -1.f) hard = -1.f;
            acc += mixG_[0] * hard;
        }

        // tanh soft clip (TanhSoftClip)
        if (en & (1u << 1)) {
            acc += mixG_[1] * NautFast::tanh((vb + fbG_[1] * fb) * tanhDrive_);
        }

        // variable-knee clip (VariableKneeClip): tanh(k*x*drive) / tanh(k)
        if (en & (1u << 2)) {
            acc += mixG_[2] * NautFast::tanh((vb + fbG_[2] * fb) * kneeDriveK_) * kneeNorm_;
        }

        // diode clip (DiodeClip): +(1 - e^(-x/etaPos)), -(1 - e^(x/etaNeg))
        if (en & (1u << 3)) {
            const float dx = (v + fbG_[3] * fb) * diodeDrive_;
            const float diode = (dx >= 0.f)
                ? 1.f - NautFast::exp2(-dx * diodeKPos_)
                : NautFast::exp2(dx * diodeKNeg_) - 1.f;
            acc += mixG_[3] * diode;
        }

        // octave fuzz: half -> full-wave rectifier into tanh
        if (en & (1u << 4)) {
            float ox = (v + fbG_[4] * fb) * octDrive_;
            ox = (ox > 0.f) ? ox : -octRect_ * ox;
            acc += mixG_[4] * NautFast::tanh(ox);
        }

        // asymmetric power shaper (AsymPowerShaper): sign(x) * min(|x|^e, 1)
        if (en & (1u << 5)) {
            float ax = v + fbG_[5] * fb;
            if (ax > 1.f) ax = 1.f;
            else if (ax < -1.f) ax = -1.f;
            float am = ax < 0.f ? -ax : ax;
            if (am < asymFloor_) am = asymFloor_;
            float ap = NautFast::exp2((ax >= 0.f ? asymPosExp_ : asymNegExp_) * NautFast::log2(am));
            if (ap > 1.f) ap = 1.f;
            acc += mixG_[5] * ((ax >= 0.f) ? ap : -ap);
        }

        // sine folder (SineShaper): sin(2pi*gain*sin(2pi*x) + asym)
        if (en & (1u << 6)) {
            const float s1 = NautFast::sinTurns((v + fbG_[6] * fb) * sineIn_);
            acc += mixG_[6] * NautFast::sinTurns(s1 * sineGain_ + sineAsymTurns_);
        }

        // Chebyshev harmonic mixer T2..T7 (ChebyshevShaper); gains prescaled by 1/6.
        if (en & (1u << 7)) {
            float c = (v + fbG_[7] * fb) * chebyIn_;
            if (c > 1.f) c = 1.f;
            else if (c < -1.f) c = -1.f;
            const float c2 = c + c;
            const float t2 = c2 * c - 1.f;
            const float t3 = c2 * t2 - c;
            const float t4 = c2 * t3 - t2;
            const float t5 = c2 * t4 - t3;
            const float t6 = c2 * t5 - t4;
            const float t7 = c2 * t6 - t5;
            acc += mixG_[7] * (chebyG_[0] * t2 + chebyG_[1] * t3 + chebyG_[2] * t4
                             + chebyG_[3] * t5 + chebyG_[4] * t6 + chebyG_[5] * t7);
        }

#if NAUTLINEAR_TABLE
        // table shaper: bilinear (level row, x) read from the current (z, w) slice.
        // The input goes through a soft saturator into the table's [-1, 1] range: a
        // hard clamp at the table edges (where the curves are steepest) aliased badly.
        if (en & kEnTable) {
            const float tx = NautFast::tanh((v + fbG_[8] * fb) * tableIn_);
            const float fx = (tx + 1.f) * tabXScale_;
            size_t i = static_cast<size_t>(fx);
            if (i > kTabX - 2) i = kTabX - 2;
            const float f = fx - static_cast<float>(i);
            const float a = tabRow0_[i] + f * (tabRow0_[i + 1] - tabRow0_[i]);
            const float b = tabRow1_[i] + f * (tabRow1_[i + 1] - tabRow1_[i]);
            acc += mixG_[8] * (a + tabFy_ * (b - a));
        }
#endif

        // normalise to a unit-weight blend, then the damper: slew-rate limit and
        // soft saturation. When off it tracks the signal, so it comes back cleanly.
        acc *= invGSum_;
        float d = acc - slewY_;
        if (en & kEnDamp) {
            if (d > slewMax_) d = slewMax_;
            else if (d < -slewMax_) d = -slewMax_;
            slewY_ += d;
            return NautFast::tanh(slewY_ * dampSat_) * dampSatNorm_;
        }
        slewY_ = acc;
        return acc;
    }

#if NAUTLINEAR_TABLE
    // Build the 4D table: one curve per (w, z, y) from a harmonic recipe. Runs once
    // at setup (shared by every instance).
    static void buildTable() {
        if (tableReady_) return;
        const float kCentre[kTabZ] = { 2.f, 4.f, 7.f, 12.f };   // harmonic centre per z
        const float kOdd[kTabW]    = { 1.f, 1.f, 0.35f };       // odd-harmonic weight per w
        const float kEven[kTabW]   = { 0.f, 1.f, 1.f };         // even-harmonic weight per w
        std::vector<float> curve(kTabX);
        float amp[kTabHarm + 1];
        for (size_t w = 0; w < kTabW; ++w)
        for (size_t z = 0; z < kTabZ; ++z)
        for (size_t y = 0; y < kTabY; ++y) {
            const float lvl = static_cast<float>(y) / static_cast<float>(kTabY - 1);
            const float rich = 0.1f + 0.9f * powf(lvl, 1.5f);   // harmonic content grows with level
            const float centre = kCentre[z] * (1.f + 0.25f * lvl);  // and brightens a little
            amp[0] = 0.f;
            amp[1] = 1.f;                                        // keep the fundamental
            for (size_t n = 2; n <= kTabHarm; ++n) {
                const float o = log2f(static_cast<float>(n) / centre) / 0.7f;  // ~0.7-octave spread
                amp[n] = rich * expf(-0.5f * o * o) * ((n & 1u) ? kOdd[w] : kEven[w]);
            }
            // f(0) = sum over even n of amp[n] * Tn(0), Tn(0) = (-1)^(n/2): subtract it so
            // silence stays silent.
            float f0 = 0.f;
            for (size_t n = 2; n <= kTabHarm; n += 2) f0 += ((n / 2) & 1u) ? -amp[n] : amp[n];
            float peak = 1e-9f;
            for (size_t xi = 0; xi < kTabX; ++xi) {
                const float x = -1.f + 2.f * static_cast<float>(xi) / static_cast<float>(kTabX - 1);
                float t0 = 1.f, t1 = x, f = amp[1] * x;
                for (size_t n = 2; n <= kTabHarm; ++n) {
                    const float t = 2.f * x * t1 - t0;
                    f += amp[n] * t;
                    t0 = t1; t1 = t;
                }
                f -= f0;
                curve[xi] = f;
                if (fabsf(f) > peak) peak = fabsf(f);
            }
            const float scale = 32767.f / peak;
            for (size_t xi = 0; xi < kTabX; ++xi)
                table_[w][z][y][xi] = static_cast<int16_t>(lrintf(curve[xi] * scale));
        }
        tableReady_ = true;
    }

    // Choose the four (z, w) corner curves and weights for the slice blend.
    void setTableCorners(float tz, float tw) {
        tabZ_ = tz; tabW_ = tw;
        size_t z0 = static_cast<size_t>(tz); if (z0 > kTabZ - 2) z0 = kTabZ - 2;
        size_t w0 = static_cast<size_t>(tw); if (w0 > kTabW - 2) w0 = kTabW - 2;
        tabFz_ = tz - static_cast<float>(z0);
        tabFw_ = tw - static_cast<float>(w0);
        tabA_ = &table_[w0][z0][0][0];
        tabB_ = &table_[w0][z0 + 1][0][0];
        tabC_ = &table_[w0 + 1][z0][0][0];
        tabD_ = &table_[w0 + 1][z0 + 1][0][0];
    }

    // Blend kSlicePointsPerSample more points of the slice from the corner curves.
    __force_inline void rebuildSlice() {
        float* out = &slice_[0][0];
        size_t pos = slicePos_;
#pragma GCC unroll 8
        for (size_t k = 0; k < kSlicePointsPerSample; ++k) {
            const float a = tabA_[pos], b = tabB_[pos], c = tabC_[pos], d = tabD_[pos];
            const float ab = a + tabFz_ * (b - a);
            const float cd = c + tabFz_ * (d - c);
            out[pos] = (ab + tabFw_ * (cd - ab)) * tabInt16Scale_;
            pos = (pos + 1) & (kSliceSize - 1);
        }
        slicePos_ = pos;
        sliceSweep_ = (sliceSweep_ > static_cast<int32_t>(kSlicePointsPerSample))
                    ? sliceSweep_ - static_cast<int32_t>(kSlicePointsPerSample) : 0;
    }
#endif

    // exp map of a 0..1 param onto [lo, hi].
    static float expMap(float p, float lo, float hi) {
        return lo * powf(hi / lo, p);
    }
    static float dbToLin(float db) {
        return expf(db * 0.1151292546f);  // ln(10)/20
    }

    // Control-rate update: smoothing, param mapping, filter coefficients, makeup.
    void updateControl() {
        using namespace NautLinearParams;
        smoother.Process(neuralNetOutputs.data(), smoothParams.data());
        const float* p = smoothParams.data();

        // --- Pre ---
        // auto-level: scale the input towards a fixed mean-abs level (max +24 dB).
        constexpr float kTarget = 0.35f;
        const float env = inEnvSlow_ > (kTarget / 16.f) ? inEnvSlow_ : (kTarget / 16.f);
        const float norm = kTarget / env;
        preGain_ = 1.f + p[AUTOLEVEL] * (norm - 1.f);

        // preamp
        preDrive_ = dbToLin(p[PRE_DRIVE] * 30.f);
        preBias_  = (p[PRE_ASYM] - 0.5f) * 2.f;
        preSagK_  = p[PRE_SAG] * preDrive_;  // shift by up to the driven envelope level
        sagRelCoef_ = 1.f - expf(-1.f / (expMap(p[PRE_SAG_REC], 0.02f, 1.f) * sampleRate_));

        const float master = dbToLin(p[DRIVE] * 30.f);
        auto trim = [](float t) { return dbToLin((t - 0.5f) * 24.f); };
        bias_ = (p[BIAS] - 0.5f) * 0.8f;

        // --- Shape ---
        constexpr float kLog2e = 1.4426950409f;
        hardDrive_ = master * trim(p[HARD_TRIM]);
        tanhDrive_ = master * trim(p[TANH_TRIM]);
        const float k = 1.f + p[KNEE_HARDNESS] * 49.f;  // knee hardness 1..50
        kneeDriveK_ = master * trim(p[KNEE_TRIM]) * k;
        kneeNorm_ = 1.f / tanhf(k);
        diodeDrive_ = master * trim(p[DIODE_TRIM]);
        diodeKPos_ = kLog2e / (0.05f + p[DIODE_ETA_POS] * 1.95f);
        diodeKNeg_ = kLog2e / (0.05f + p[DIODE_ETA_NEG] * 1.95f);
        octDrive_ = master * trim(p[OCT_TRIM]);
        octRect_  = p[OCT_RECTIFY];
        asymPosExp_ = p[ASYM_EXP_POS] * 6.f - 3.f;
        asymNegExp_ = p[ASYM_EXP_NEG] * 6.f - 3.f;
        sineIn_ = expMap(p[SINE_IN], 0.1f, 2.f);
        sineGain_ = p[SINE_GAIN];
        sineAsymTurns_ = p[SINE_ASYM] * 0.5f * 0.1591549431f;  // asym halved (as MEMLCelium), radians -> turns
        chebyIn_ = expMap(p[CHEBY_IN], 0.25f, 2.f);
        for (size_t h = 0; h < kChebyHarmonics; ++h) {
            chebyG_[h] = (p[CHEBY_T2 + h] * 2.f - 1.f) * (1.f / kChebyHarmonics);
        }
#if NAUTLINEAR_TABLE
        tableIn_ = expMap(p[TABLE_IN], 0.25f, 2.f);
        {
            // new (z, w): restart a sweep so the whole slice follows
            const float tz = p[TABLE_HARM] * static_cast<float>(kTabZ - 1);
            const float tw = p[TABLE_OE] * static_cast<float>(kTabW - 1);
            if (fabsf(tz - tabZ_) > 0.003f || fabsf(tw - tabW_) > 0.003f) {
                setTableCorners(tz, tw);
                sliceSweep_ = static_cast<int32_t>(kSliceSize);
            }
        }
#endif

        // --- Enables --- (snapshot once per control tick for the per-sample path)
        enMask_ = enableMask_;

        // --- Mix --- disabled shapers drop out of the blend normalisation.
        float gSum = 0.f;
        for (size_t i = 0; i < kNShapers; ++i) {
            mixG_[i] = (enMask_ & (1u << i)) ? p[MIX + i] : 0.f;
            gSum += mixG_[i];
        }
        invGSum_ = 1.f / (gSum > 1e-3f ? gSum : 1e-3f);
        fade_ = gSum < 1.f ? gSum : 1.f;

        // --- FB ---
        const float fbMaster = p[FB_MASTER] * p[FB_MASTER] * 2.f;
        for (size_t i = 0; i < kNShapers; ++i) {
            fbG_[i] = fbMaster * (p[FB_SHAPER + i] * 2.f - 1.f);
        }

        // --- Damp ---
        // max change per oversampled sample: 2.0 (never limits ±1 audio) .. 0.002
        // (a full-scale sine is limited above ~60 Hz).
        slewMax_ = 2.f * powf(0.001f, p[DAMP_SLEW]);
        dampSat_ = dbToLin(p[DAMP_SAT] * 18.f);
        dampSatNorm_ = 1.f / tanhf(dampSat_);

        // makeup: wet loudness -> dry loudness, -12 .. +18 dB. The floor keeps a
        // self-oscillating feedback loop audible when the dry input is silent.
        float target = dryEnv_ / (wetEnv_ + 1e-6f);
        if (target > 8.f) target = 8.f;
        else if (target < 0.25f) target = 0.25f;
        makeup_ += makeupCoef_ * (target - makeup_);

        // equal-power wet/dry, RVY1 knob via wetdry_mix_.
        wetG_ = sqrtf(wetdry_mix_);
        dryG_ = sqrtf(1.f - wetdry_mix_);
    }

    std::array<float,NPARAMS> neuralNetOutputs{0}, smoothParams{0};

    float wetdry_mix_{0.5f};
    float wetdryKnobValue{0.5f};
    float sampleRate_{48000.f};

    // smoothing runs at control rate
    OnePoleSmoother<kN_Params> smoother{150.f, (float)kSampleRate / kCtrlInterval};
    size_t ctrlCount_{0};

    // control-rate outputs read by the per-sample path (members -> SRAM)
    float preGain_{1.f};
    float bias_{0.f};
    float hardDrive_{1.f}, tanhDrive_{1.f};
    float kneeDriveK_{1.f}, kneeNorm_{1.f};
    float diodeDrive_{1.f}, diodeKPos_{1.f}, diodeKNeg_{1.f};
    float octDrive_{1.f}, octRect_{1.f};
    float asymPosExp_{1.f}, asymNegExp_{1.f};
    float sineIn_{1.f}, sineGain_{0.f}, sineAsymTurns_{0.f};
    float chebyIn_{1.f};
    std::array<float, NautLinearParams::kChebyHarmonics> chebyG_{};
    std::array<float, kNShapers> mixG_{};
    uint32_t enMask_{kEnableAll};      // control-rate snapshot of enableMask_
    std::array<float, kNShapers> fbG_{};  // master * per-shaper feedback gain
    float fbLast_{0.f};                // last wet sample (feedback tap)
    float fbInterp_{0.f}, fbStep_{0.f};
    float invGSum_{1.f};
    float fade_{0.f};
    float makeup_{1.f};
    float wetG_{0.7071f}, dryG_{0.7071f};

#if NAUTLINEAR_TABLE
    // table shaper (table_ and slice_ are static: SRAM .bss, shared by instances)
    inline static int16_t table_[kTabW][kTabZ][kTabY][kTabX];
    inline static float   slice_[kTabY][kTabX];
    inline static bool    tableReady_ = false;
    const int16_t* tabA_{nullptr}; const int16_t* tabB_{nullptr};
    const int16_t* tabC_{nullptr}; const int16_t* tabD_{nullptr};
    float tabZ_{-1.f}, tabW_{-1.f}, tabFz_{0.f}, tabFw_{0.f};
    size_t slicePos_{0};
    int32_t sliceSweep_{0};
    const float* tabRow0_{&slice_[0][0]};
    const float* tabRow1_{&slice_[1][0]};
    float tabFy_{0.f};
    float tableIn_{1.f};
    float tabInt16Scale_{1.f / 32767.f};
    float tabXScale_{0.5f * static_cast<float>(kTabX - 1)};
    float tabYScale_{1.f / 0.7f};             // envelope 0.7 -> top level row
    float tabYMax_{static_cast<float>(kTabY - 1)};
#endif

    // envelopes
    float inEnvSlow_{0.f}, dryEnv_{0.f}, wetEnv_{0.f};
    float envFastCoef_{0.f}, envSlowCoef_{0.f}, makeupCoef_{0.f};

    // per-sample constants kept in SRAM
    float dcCoef_{0.995f};
    float wetLimit_{1.f};
    float outLimDrive_{1.2f}, outLimNorm_{1.f / 1.2f};
    float asymFloor_{1e-4f};
    float fbStepScale_{0.25f};  // 1 / oversampling factor

    // preamp
    float preDrive_{1.f}, preBias_{0.f}, preSagK_{0.f};
    float sagEnv_{0.f}, sagAtkCoef_{0.f}, sagRelCoef_{0.f};
    float preBiasNow_{0.f}, preBiasOff_{0.f};

    // damper
    float slewMax_{2.f}, slewY_{0.f};
    float dampSat_{1.f}, dampSatNorm_{1.3130353f};  // 1 / tanh(1)
    Oversampler<4> bankOS_;  // one oversampler for the whole shaper bank
    maxiDCBlocker wetDC_;

};

#endif
