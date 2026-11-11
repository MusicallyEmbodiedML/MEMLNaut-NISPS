#ifndef __NAUTLINEAR_AUDIO_APP_HPP__
#define __NAUTLINEAR_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <cmath>

#include "../../src/memllib/synth/OnePoleSmoother.hpp"
#include "../../src/memllib/synth/Oversampler.hpp"
#include "../../src/memllib/synth/maximilian.h"
#include "../../src/memllib/synth/SubbandFlux.hpp"
#include "../../src/memllib/synth/GrainDelayI16.hpp"

// ---------------------------------------------------------------------------
// NautLinear — NN-driven parallel waveshaper bank.
//
// Signal chain (mono):
//
//   in L+R -> auto-level -> pre-EQ (peaking) -> [4x oversampled: 8 shapers in
//   parallel -> weighted mix] -> DC block -> post resonant lowpass
//   -> auto makeup gain (wet matched to dry loudness) -> wet/dry (RVY1)
//
// Feedback: the wet mix is fed back one sample late, scaled by a master
// feedback gain and then by a separate bipolar gain per shaper, into each
// shaper's input. Inside the oversampler it is linearly interpolated across the
// sub-samples. The tap point is a per-model option (fbPreMakeup_):
//   pre-makeup  (default): after the post filter, before makeup gain and the
//                mix fade, clamped to ±1. The loop can sustain and
//                self-oscillate without input.
//   post-makeup: the final wet signal (after makeup, fade and limiter). Makeup
//                follows the dry input, so the loop mostly reshapes tone and
//                dies away with the input.
// Makeup has a -12 dB floor so a self-oscillating loop stays audible when the
// dry input is silent.
//
// Grain delay (as ChunkyBits' grain delays): the pre-EQ'd input feeds a
// 2-grain delay (length, start time, feedback, pitch, pitch spread). Its output
// is sent into each shaper's input with a separate amount, alongside the
// feedback, and is likewise interpolated across the oversampled sub-samples.
// The feedback tap can also be sent into the grain delay (GRAIN_FB_SEND), so
// the shaped sound is re-grained.
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
// block ~6x. All parameter mapping, filter coefficients and makeup gain run
// at control rate (every kCtrlInterval samples).
// ---------------------------------------------------------------------------

// Param map (NN outputs, 0..1). Sections match the RL screen group labels.
namespace NautLinearParams {
    constexpr size_t kChebyHarmonics = 6;  // T2..T7
    enum : size_t {
        // --- Pre ---
        AUTOLEVEL = 0,   // 0 = fixed input gain .. 1 = fully level-normalised input
        PRE_FREQ,        // pre-EQ peak frequency, 80 Hz .. 6 kHz (exp)
        PRE_GAIN,        // pre-EQ peak gain, -18 .. +18 dB (0.5 = flat)
        PRE_Q,           // pre-EQ Q, 0.5 .. 8 (exp)
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
        // --- Mix --- (order: hard, tanh, knee, diode, octave, asym, sine, cheby)
        MIX = CHEBY_T2 + kChebyHarmonics,
        // --- Post ---
        POST_CUTOFF = MIX + 8,  // post lowpass cutoff, 200 Hz .. 16 kHz (exp)
        POST_RES,               // post lowpass Q, 0.5 .. 6 (exp)
        // --- FB ---
        FB_MASTER,              // master feedback gain, 0 .. 2 (squared curve)
        FB_SHAPER,              // per-shaper feedback gains, -1 .. 1, same order as MIX
        // --- Grain --- (mappings as ChunkyBits)
        GRAIN_LEN = FB_SHAPER + 8,  // grain length, 10 .. 500 ms
        GRAIN_START,            // start time (delay), 10 .. 330 ms
        GRAIN_FB,               // grain delay feedback, 0 .. 0.95
        GRAIN_PITCH,            // pitch ratio, 0.5 .. 2 (exp)
        GRAIN_SPREAD,           // pitch spread between the 2 grains, 0 .. 0.3
        GRAIN_SEND,             // per-shaper grain send amounts, 0 .. 1, same order as MIX
        GRAIN_FB_SEND = GRAIN_SEND + 8,  // feedback tap -> grain delay input, 0 .. 1
        COUNT
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

// Transposed direct form II biquad. Coefficients set at control rate.
struct NautBiquad {
    float b0{1.f}, b1{0.f}, b2{0.f}, a1{0.f}, a2{0.f};
    float z1{0.f}, z2{0.f};

    __force_inline float play(float x) {
        const float y = b0 * x + z1;
        z1 = b1 * x - a1 * y + z2;
        z2 = b2 * x - a2 * y;
        return y;
    }

    // RBJ cookbook peaking EQ.
    void setPeak(float freq, float gainDb, float q, float fs) {
        const float A = powf(10.f, gainDb * (1.f / 40.f));
        const float w0 = 6.28318530718f * freq / fs;
        const float c = cosf(w0);
        const float alpha = sinf(w0) / (2.f * q);
        const float inv = 1.f / (1.f + alpha / A);
        b0 = (1.f + alpha * A) * inv;
        b1 = (-2.f * c) * inv;
        b2 = (1.f - alpha * A) * inv;
        a1 = b1;
        a2 = (1.f - alpha / A) * inv;
    }

    // RBJ cookbook lowpass.
    void setLowpass(float freq, float q, float fs) {
        const float w0 = 6.28318530718f * freq / fs;
        const float c = cosf(w0);
        const float alpha = sinf(w0) / (2.f * q);
        const float inv = 1.f / (1.f + alpha);
        b0 = (0.5f * (1.f - c)) * inv;
        b1 = (1.f - c) * inv;
        b2 = b0;
        a1 = (-2.f * c) * inv;
        a2 = (1.f - alpha) * inv;
    }
};

template<size_t NPARAMS = NautLinearParams::COUNT>
class NautLinearAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t kCtrlInterval = 32;  // samples per control-rate update (0.67 ms)

    queue_t wetdryQueue;

    // Feedback tap point (Options screen, saved per model): true = before makeup gain.
    volatile bool fbPreMakeup_{true};

    // Enable screen (saved per model). Bits 0..7 = shapers in MIX order (hard,
    // tanh, knee, diode, octave, asym, sine, cheby), bit 8 = feedback, bit 9 =
    // grain delay. Disabled blocks are skipped, not just muted (saves CPU).
    static constexpr uint32_t kEnFeedback = 1u << 8;
    static constexpr uint32_t kEnDelay    = 1u << 9;
    static constexpr uint32_t kEnableAll  = 0x3ffu;
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

        // auto-level, then pre-EQ, then the oversampled shaper bank.
        const float s = preEQ_.play(mono * preGain_);
        // feedback ramps from where it is to last sample's wet over the 4 sub-samples.
        fbStep_ = (fbLast_ - fbInterp_) * fbStepScale_;
        // grain delay on the shaper input; its output ramps in the same way.
        const float grain = (enMask_ & kEnDelay) ? grain_.process(s, fbLast_ * grainFbSend_) : 0.f;
        grainStep_ = (grain - grainInterp_) * fbStepScale_;
        const float mixed = bankOS_.process(s, [this](float v) __attribute__((always_inline)) {
            return shapeAll(v);
        });

        // normalise to a unit-weight blend (the overall level is applied by
        // makeup * fade below), remove DC (bias, rectifier, even harmonics),
        // then the post lowpass.
        float blend = mixed * invGSum_;
        blend = wetDC_.play(blend, dcCoef_);
        blend = postLP_.play(blend);

        // makeup gain matches the wet loudness to the dry input, so param changes
        // are heard as tone rather than level; fade_ = min(sum of mix levels, 1)
        // lets the net still pull the wet signal down.
        wetEnv_ += envFastCoef_ * (fabsf(blend) - wetEnv_);
        float wet = blend * makeup_ * fade_;
        if (wet > wetLimit_) wet = wetLimit_;
        else if (wet < -wetLimit_) wet = -wetLimit_;

        // feedback tap. Pre-makeup is clamped (a resonant post filter can exceed ±1);
        // post-makeup is already limited.
        float fbTap = fbPreMakeup_ ? blend : wet;
        if (fbTap > wetLimit_) fbTap = wetLimit_;
        else if (fbTap < -wetLimit_) fbTap = -wetLimit_;
        fbLast_ = (enMask_ & kEnFeedback) ? fbTap : 0.f;

        const float y = (wet * wetG_) + (mono * dryG_);

        // machine-listen on the OUTPUT — fills outputFlux.{bandEnv,bandFlux,fluxTotal}
        // for later feedback mapping into the signal chain.
        outputFlux.process(y);

        stereosample_t ret { y, y };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRate_ = sample_rate;
        outputFlux.setup(sample_rate);  // filters need maxiSettings::sampleRate set first

        envFastCoef_ = 1.f - expf(-1.f / (0.08f * sample_rate));   // 80 ms
        envSlowCoef_ = 1.f - expf(-1.f / (0.3f * sample_rate));    // 300 ms
        const float ctrlRate = sample_rate / static_cast<float>(kCtrlInterval);
        makeupCoef_ = 1.f - expf(-1.f / (0.05f * ctrlRate));       // 50 ms
        smoother.Setup(150.f, ctrlRate);  // smoothing runs at control rate
        grain_.setup(sample_rate);
        grain_.setClampWrite(true);  // int16 buffer: the pre-EQ'd input can exceed ±1
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

    // All eight shapers on one oversampled sample, returned as the weighted mix.
    // Every per-shaper constant is precomputed in updateControl().
    __force_inline float shapeAll(float v) {
        // feedback, interpolated across the oversampled sub-samples
        fbInterp_ += fbStep_;
        const float fb = fbInterp_;
        grainInterp_ += grainStep_;
        const float gr = grainInterp_;

        const float vb = v + bias_;  // biased feed for the symmetric clippers
        const uint32_t en = enMask_;
        float acc = 0.f;

        // hard clip (HardClip, ceiling 1)
        if (en & (1u << 0)) {
            float hard = (vb + fbG_[0] * fb + grG_[0] * gr) * hardDrive_;
            if (hard > 1.f) hard = 1.f;
            else if (hard < -1.f) hard = -1.f;
            acc += mixG_[0] * hard;
        }

        // tanh soft clip (TanhSoftClip)
        if (en & (1u << 1)) {
            acc += mixG_[1] * NautFast::tanh((vb + fbG_[1] * fb + grG_[1] * gr) * tanhDrive_);
        }

        // variable-knee clip (VariableKneeClip): tanh(k*x*drive) / tanh(k)
        if (en & (1u << 2)) {
            acc += mixG_[2] * NautFast::tanh((vb + fbG_[2] * fb + grG_[2] * gr) * kneeDriveK_) * kneeNorm_;
        }

        // diode clip (DiodeClip): +(1 - e^(-x/etaPos)), -(1 - e^(x/etaNeg))
        if (en & (1u << 3)) {
            const float dx = (v + fbG_[3] * fb + grG_[3] * gr) * diodeDrive_;
            const float diode = (dx >= 0.f)
                ? 1.f - NautFast::exp2(-dx * diodeKPos_)
                : NautFast::exp2(dx * diodeKNeg_) - 1.f;
            acc += mixG_[3] * diode;
        }

        // octave fuzz: half -> full-wave rectifier into tanh
        if (en & (1u << 4)) {
            float ox = (v + fbG_[4] * fb + grG_[4] * gr) * octDrive_;
            ox = (ox > 0.f) ? ox : -octRect_ * ox;
            acc += mixG_[4] * NautFast::tanh(ox);
        }

        // asymmetric power shaper (AsymPowerShaper): sign(x) * min(|x|^e, 1)
        if (en & (1u << 5)) {
            float ax = v + fbG_[5] * fb + grG_[5] * gr;
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
            const float s1 = NautFast::sinTurns((v + fbG_[6] * fb + grG_[6] * gr) * sineIn_);
            acc += mixG_[6] * NautFast::sinTurns(s1 * sineGain_ + sineAsymTurns_);
        }

        // Chebyshev harmonic mixer T2..T7 (ChebyshevShaper); gains prescaled by 1/6.
        if (en & (1u << 7)) {
            float c = (v + fbG_[7] * fb + grG_[7] * gr) * chebyIn_;
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

        return acc;
    }

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

        preEQ_.setPeak(expMap(p[PRE_FREQ], 80.f, 6000.f),
                       (p[PRE_GAIN] - 0.5f) * 36.f,
                       expMap(p[PRE_Q], 0.5f, 8.f), sampleRate_);

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

        // --- Enables --- (snapshot once per control tick for the per-sample path)
        enMask_ = enableMask_;

        // --- Mix --- disabled shapers drop out of the blend normalisation.
        float gSum = 0.f;
        for (size_t i = 0; i < 8; ++i) {
            mixG_[i] = (enMask_ & (1u << i)) ? p[MIX + i] : 0.f;
            gSum += mixG_[i];
        }
        invGSum_ = 1.f / (gSum > 1e-3f ? gSum : 1e-3f);
        fade_ = gSum < 1.f ? gSum : 1.f;

        // --- FB ---
        const float fbMaster = p[FB_MASTER] * p[FB_MASTER] * 2.f;
        for (size_t i = 0; i < 8; ++i) {
            fbG_[i] = fbMaster * (p[FB_SHAPER + i] * 2.f - 1.f);
        }

        // --- Grain ---
        grain_.setGrainLengthMs(10.f + p[GRAIN_LEN] * 490.f);
        grain_.setStartTimeMs(10.f + p[GRAIN_START] * 320.f);
        grain_.setFeedback(p[GRAIN_FB] * 0.95f);
        grain_.setPitch(exp2f(p[GRAIN_PITCH] * 2.f - 1.f));
        grain_.setPitchSpread(p[GRAIN_SPREAD] * 0.3f);
        for (size_t i = 0; i < 8; ++i) grG_[i] = p[GRAIN_SEND + i];
        grainFbSend_ = p[GRAIN_FB_SEND];

        // --- Post ---
        postLP_.setLowpass(expMap(p[POST_CUTOFF], 200.f, 16000.f),
                           expMap(p[POST_RES], 0.5f, 6.f), sampleRate_);

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
    std::array<float, 8> mixG_{};
    uint32_t enMask_{kEnableAll};      // control-rate snapshot of enableMask_
    std::array<float, 8> fbG_{};       // master * per-shaper feedback gain
    float fbLast_{0.f};                // last wet sample (feedback tap)
    float fbInterp_{0.f}, fbStep_{0.f};
    std::array<float, 8> grG_{};       // per-shaper grain send
    float grainInterp_{0.f}, grainStep_{0.f};
    float grainFbSend_{0.f};           // feedback tap -> grain delay
    float invGSum_{1.f};
    float fade_{0.f};
    float makeup_{1.f};
    float wetG_{0.7071f}, dryG_{0.7071f};

    // envelopes
    float inEnvSlow_{0.f}, dryEnv_{0.f}, wetEnv_{0.f};
    float envFastCoef_{0.f}, envSlowCoef_{0.f}, makeupCoef_{0.f};

    // per-sample constants kept in SRAM
    float dcCoef_{0.995f};
    float wetLimit_{1.f};
    float asymFloor_{1e-4f};
    float fbStepScale_{0.25f};  // 1 / oversampling factor

    NautBiquad preEQ_, postLP_;
    Oversampler<4> bankOS_;  // one oversampler for the whole shaper bank
    maxiDCBlocker wetDC_;
    GrainDelayI16<16384, 2> grain_;  // pre-shaper grain delay (as ChunkyBits)

    // machine listening on the output (feedback features for later mapping)
    SubbandFlux<3> outputFlux;

};

#endif
