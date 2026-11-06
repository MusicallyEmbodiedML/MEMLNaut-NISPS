#ifndef __ONE_SHOT_SAMPLER_HPP__
#define __ONE_SHOT_SAMPLER_HPP__

#include <cstddef>
#include <cstdint>

// Plays a 16-bit sample once per trigger, from the start, with linear interpolation. A new
// trigger cuts off the one before. process() and trigger() run on the audio core; the
// setters may run on the control core.
class OneShotSampler {
public:
    void setup(const int16_t* samples, size_t length, float fileRate, float outputRate) {
        samples_ = samples;
        last_ = static_cast<float>(length) - 1.f;
        rate_ = fileRate / outputRate;  // plays at the sample's original pitch
        end_ = last_;
    }

    // Length of the hit, relative to the sample. It holds for the first half of the length
    // and fades out over the second, so 2 plays the sample untouched.
    void setLength(float length) { end_ = last_ * length; }

    void trigger(float level) {
        level_ = level;
        step_ = 0;
        active_ = true;
    }

    __force_inline float process() {
        if (!active_) return 0.f;
        const float end = end_;
        // Multiplied, not accumulated: adding to a float phase rounds on every step, and
        // the error builds up over long samples.
        const float phase = static_cast<float>(step_) * rate_;
        if (phase >= last_ || phase >= end) {
            active_ = false;
            return 0.f;
        }
        const size_t i = static_cast<size_t>(phase);
        const float frac = phase - static_cast<float>(i);
        const float s0 = static_cast<float>(samples_[i]);
        const float s1 = static_cast<float>(samples_[i + 1]);
        const float fade = (end - phase) * 2.f / end;
        const float out = (s0 + (s1 - s0) * frac) * kScale * level_ * (fade < 1.f ? fade : 1.f);
        step_++;
        return out;
    }

private:
    static constexpr float kScale = 1.f / 32768.f;
    const int16_t* samples_ = nullptr;
    float last_ = 0.f;  // index of the last sample
    float rate_ = 1.f;
    volatile float end_ = 0.f;
    uint32_t step_ = 0;
    float level_ = 1.f;
    bool active_ = false;
};

#endif  // __ONE_SHOT_SAMPLER_HPP__
