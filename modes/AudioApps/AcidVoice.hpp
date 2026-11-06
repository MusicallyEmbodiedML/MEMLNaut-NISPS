#ifndef __ACID_VOICE_HPP__
#define __ACID_VOICE_HPP__

#include <cmath>

// A cheap 303-style bass voice: saw or square into a four-pole resonant lowpass, with a
// filter envelope, accent and slide. noteOn/noteOff/process run on the audio core; the
// setters may run on the control core.
class AcidVoice {
public:
    void setup(float sampleRate) {
        radPerSample_ = 2.f * static_cast<float>(M_PI) / sampleRate;
        hzToInc_ = 1.f / sampleRate;
        sampleRate_ = sampleRate;
        glideCoef_ = attackCoef(0.06f);
        ampCoef_ = attackCoef(0.003f);
        ampHoldMul_ = decayMul(1.5f);
        accentMul_ = decayMul(0.2f);
        hpCoef_ = 1.f - expf(-radPerSample_ * 250.f);
        setDecay(0.2f);
        setCutoff(400.f);
        g_ = gTarget_;
    }

    void setCutoff(float hz) { cutoffHz_ = hz; }
    // 0..1. Never fully clean, self-oscillating from about 2/3 up, and screaming at the top.
    void setResonance(float amount) { k_ = kMinFeedback + amount * amount * (kMaxFeedback - kMinFeedback); }
    void setEnvAmount(float octaves) { envOctaves_ = octaves; }
    void setDecay(float seconds) { filterMul_ = decayMul(seconds); }
    void setAccent(float amount) { accentAmount_ = amount; }  // 0..1
    void setSquare(bool square) { square_ = square; }

    // slide: glide from the current pitch without retriggering the envelopes. It only
    // applies while the previous note is still held; otherwise this is an ordinary note.
    void noteOn(float hz, bool accent, bool slide) {
        targetInc_ = hz * hzToInc_;
        accentCharge_ = accent ? fminf(accentCharge_ + 0.5f, 1.f) : accentCharge_ * 0.5f;
        ampTarget_ = accent ? kNormalLevel + (1.f - kNormalLevel) * accentAmount_ : kNormalLevel;
        if (slide && gate_) return;

        inc_ = targetInc_;
        filterEnv_ = 1.f;
        activeFilterMul_ = accent ? accentMul_ : filterMul_;
        if (accent) accentEnv_ = accentCharge_;
        gate_ = true;
    }

    void noteOff() {
        gate_ = false;
        ampTarget_ = 0.f;
    }

    static __force_inline float softClip(float x) {
        if (x > 3.f) return 1.f;
        if (x < -3.f) return -1.f;
        const float x2 = x * x;
        return x * (27.f + x2) / (27.f + 9.f * x2);
    }

    __force_inline float process() {
        if (ampTarget_ < kSilence && amp_ < kSilence) {
            ampTarget_ = 0.f;
            amp_ = 0.f;
            return 0.f;
        }

        // Pitch, level and envelopes.
        inc_ += (targetInc_ - inc_) * glideCoef_;
        amp_ += (ampTarget_ - amp_) * ampCoef_;
        ampTarget_ *= ampHoldMul_;
        filterEnv_ *= activeFilterMul_;
        accentEnv_ *= accentMul_;

        // The cutoff conversion is the expensive part, so it runs at a lower rate.
        if (controlCounter_ == 0) {
            const float octaves = envOctaves_ * filterEnv_ + kAccentOctaves * accentAmount_ * accentEnv_;
            const float w = fminf(cutoffHz_ * exp2f(octaves), kMaxCutoffHz) * radPerSample_;
            gTarget_ = w / (1.f + w);
        }
        controlCounter_ = (controlCounter_ + 1) & (kControlPeriod - 1);
        g_ += (gTarget_ - g_) * 0.1f;

        // Oscillator, with polyBLEP to soften the aliasing.
        phase_ += inc_;
        if (phase_ >= 1.f) phase_ -= 1.f;
        float osc;
        if (square_) {
            float half = phase_ + 0.5f;
            if (half >= 1.f) half -= 1.f;
            osc = (phase_ < 0.5f ? 1.f : -1.f) + polyBlep(phase_, inc_) - polyBlep(half, inc_);
        } else {
            osc = 2.f * phase_ - 1.f - polyBlep(phase_, inc_);
        }

        // Four one-pole stages. The feedback is highpassed and soft-clipped, which keeps
        // the bass under resonance and keeps the loop bounded. The clip sits above the
        // oscillator's level, so the resonance can shout over the note.
        hpState_ += hpCoef_ * (y4_ - hpState_);
        // The (1 + g) makes up the loop's loss at high cutoffs, where the delay in the
        // feedback lowers the gain at the resonant frequency.
        const float feedback = (y4_ - hpState_) * k_ * (1.f + g_);
        const float in = osc - softClip(feedback / kFeedbackLimit) * kFeedbackLimit;
        y1_ += g_ * (in - y1_);
        y2_ += g_ * (y1_ - y2_);
        y3_ += g_ * (y2_ - y3_);
        y4_ += g_ * (y3_ - y4_);

        return softClip(y4_ * kOutputDrive) * amp_;
    }

private:
    static constexpr float kNormalLevel = 0.6f;
    static constexpr float kAccentOctaves = 3.f;
    static constexpr float kMinFeedback = 2.f;
    static constexpr float kMaxFeedback = 10.f;
    static constexpr float kFeedbackLimit = 4.f;
    static constexpr float kOutputDrive = 2.f;
    static constexpr float kMaxCutoffHz = 10000.f;
    static constexpr float kSilence = 1e-5f;
    static constexpr unsigned kControlPeriod = 8;  // a power of two

    float attackCoef(float seconds) const { return 1.f - expf(-1.f / (seconds * sampleRate_)); }
    float decayMul(float seconds) const { return expf(-1.f / (seconds * sampleRate_)); }

    static __force_inline float polyBlep(float t, float dt) {
        if (t < dt) {
            const float x = t / dt;
            return x + x - x * x - 1.f;
        }
        if (t > 1.f - dt) {
            const float x = (t - 1.f) / dt;
            return x * x + x + x + 1.f;
        }
        return 0.f;
    }

    float sampleRate_ = 48000.f;
    float radPerSample_ = 0.f;
    float hzToInc_ = 0.f;
    float glideCoef_ = 0.f;
    float ampCoef_ = 0.f;
    float ampHoldMul_ = 1.f;
    float accentMul_ = 1.f;
    float hpCoef_ = 0.f;

    volatile float cutoffHz_ = 400.f;
    volatile float k_ = 0.f;
    volatile float envOctaves_ = 0.f;
    volatile float filterMul_ = 1.f;
    volatile float accentAmount_ = 0.f;
    volatile bool square_ = false;

    float phase_ = 0.f;
    float inc_ = 0.f;
    float targetInc_ = 0.f;
    float amp_ = 0.f;
    float ampTarget_ = 0.f;
    float filterEnv_ = 0.f;
    float activeFilterMul_ = 1.f;
    float accentEnv_ = 0.f;
    float accentCharge_ = 0.f;
    bool gate_ = false;

    float g_ = 0.f;
    float gTarget_ = 0.f;
    unsigned controlCounter_ = 0;
    float y1_ = 0.f, y2_ = 0.f, y3_ = 0.f, y4_ = 0.f;
    float hpState_ = 0.f;
};

#endif  // __ACID_VOICE_HPP__
