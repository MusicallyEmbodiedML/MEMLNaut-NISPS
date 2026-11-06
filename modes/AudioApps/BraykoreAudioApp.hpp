#ifndef __BRAYKORE_AUDIO_APP_HPP__
#define __BRAYKORE_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/interface/InterfaceBase.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "RatioSeq.hpp"
#include "RatioSeqEngine.hpp"
#include "FMPatternGen.hpp"
#include "FlashSamplePlayer.hpp"


template<size_t NPARAMS=30, size_t NSEQUENCES=2>
class BraykoreAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params   = NPARAMS;
    static constexpr size_t kNRatioSeqs = NSEQUENCES / 2;  // 1

    static constexpr uint32_t kFocusSeq = (1u << 0);
    static constexpr uint32_t kFocusFM1 = (1u << 1);
    static constexpr uint32_t kFocusFM2 = (1u << 2);
    static constexpr uint32_t kFocusFX  = (1u << 3);
    static constexpr uint32_t kFocusFM3 = (1u << 4);

    static constexpr float kTwoPi = 6.2831853f;

    static constexpr std::array<uint32_t, NPARAMS> kParamGroupMask = {
        kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq, kFocusSeq,  // 0-7
        kFocusFM1, kFocusFM1, kFocusFM1, kFocusFM1, kFocusFM1, kFocusFM1, kFocusFM1,              // 8-14
        kFocusFM2, kFocusFM2, kFocusFM2, kFocusFM2, kFocusFM2, kFocusFM2, kFocusFM2,              // 15-21
        kFocusFX,  kFocusFX,  kFocusFX,                                                            // 22-24
        kFocusFM3, kFocusFM3, kFocusFM3, kFocusFM3, kFocusFM3,                                    // 25-29
    };

    queue_t bpmQueue;
    queue_t sequencerControlQueue;

    bool sequencerPlaying = false;

    RatioSeqEngine<kNRatioSeqs, 4, 2> seqEngine;

    BraykoreAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&bpmQueue, sizeof(float), 1);
        queue_init(&sequencerControlQueue, sizeof(int), 1);
    }

    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        // Drain start/stop queue here so TogB2 works without waiting for NN params
        int ctrl;
        if (queue_try_remove(&sequencerControlQueue, &ctrl)) {
            sequencerPlaying = ctrl == 1;
            seqEngine.setPlaying(sequencerPlaying);
        }

        if (sequencerPlaying && seqEngine.tick()) {
            float bp = seqEngine.getBarPhasor();

            // FM1: slice position, quantized to 0.125 steps
            float p1 = fmodf(bp * fmState1.phasorMul, 1.f);
            float v1 = fmPair(p1, fmState1.carrierFreq, fmState1.modFreq,
                              fmState1.modIndex, fmState1.fbLevel,
                              fmState1.carrier, fmState1.modulator);
            fmValue = floorf((v1 + 1.f) * 0.5f * 8.f) * 0.125f;

            // FM2: pitch, quantized to {0.5, 1.0, 2.0}
            float p2 = fmodf(bp * fmState2.phasorMul, 1.f);
            float v2 = fmPair(p2, fmState2.carrierFreq, fmState2.modFreq,
                              fmState2.modIndex, fmState2.fbLevel,
                              fmState2.carrier, fmState2.modulator);
            static const float pitches[8] = {0.5f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 2.f};
            pitchFactor = pitches[(int)((v2 + 1.f) * 0.5f * 7.999f)];

            bool trig = seqEngine.states[0].lastTrig;
            if (trig && !prevTrig) {
                beatPlayer.trigger(fmValue);
                currentPlaybackRate = flashPlaybackRate * pitchFactor;

                // FM3: bitcrush depth, 4–16 bits, sampled at each trigger
                float p3 = fmodf(bp * fmState3.phasorMul, 1.f);
                float v3 = fmPair(p3, fmState3.carrierFreq, fmState3.modFreq,
                                  fmState3.modIndex, fmState3.fbLevel,
                                  fmState3.carrier, fmState3.modulator);
                float crushBits = 4.f + (v3 + 1.f) * 0.5f * 12.f;
                bitCrushLevels = powf(2.f, crushBits);

                Serial.printf("trig slice=%.3f pitch=%.1f bits=%.1f\n", fmValue, pitchFactor, crushBits);
            }
            prevTrig = trig;
        }
        float s = beatPlayer.process(currentPlaybackRate);
        s = roundf(s * bitCrushLevels) / bitCrushLevels;
        float shape = sinf(s * kTwoPi);
        shape = sinf(((shape * kTwoPi) * sineShapeGain) + sineShapeASym);
        s = tanhf(s + (shape * sineShapeMix));
        return { s, s };
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface)
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        sampleRatef = sample_rate;
        seqEngine.setup(sample_rate);
        seqEngine.updateBPM(currentBPM);
    }

    void loadBreakbeat(const char* name, float driverSR) {
        FlashSample s = loadFlashSample(name);
        if (!s.found) return;
        loadedSampleCount = static_cast<float>(s.sampleCount);
        beatPlayer.load(s);
        updatePlaybackRate();
    }

    void loop() override {
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        float newBPM;
        if (queue_try_remove(&bpmQueue, &newBPM)) {
            currentBPM = newBPM;
            seqEngine.updateBPM(newBPM);
            updatePlaybackRate();
        }

        // params 0-6 → ratio seq, straight intervals only {1,2,4,8}
        {
            static const float straight[4] = {1.f, 2.f, 4.f, 8.f};
            auto& seq = seqEngine.states[0];
            size_t pi = 0;
            float sum = 0.f;
            for (size_t i = 0; i < seq.ratios.size(); i++) {
                seq.ratios[i] = straight[(int)(params[pi++] * 3.999f)];
                sum += seq.ratios[i];
            }
            seq.ratioSum = sum;
            seq.phasorMul   = straight[(int)(params[pi++] * 3.999f)];
            seq.phaseOffset = ((int)(params[pi++] * timeSigBeats)) * (1.f / timeSigBeats);
            sum = 0.f;
            for (size_t i = 0; i < seq.ampRatios.size(); i++) {
                seq.ampRatios[i] = straight[(int)(params[pi++] * 3.999f)];
                sum += seq.ampRatios[i];
            }
            seq.ampRatioSum = sum;
        }

        // params 8-14 → FM1 (slice position)
        {
            size_t pi = 8;
            fmState1.carrierFreq = 1.f + params[pi++] * 3.f;
            fmState1.modFreq     = 1.f + params[pi++] * 7.f;
            fmState1.modIndex    = params[pi++] * 4.f;
            fmState1.fbLevel     = params[pi++];
            fmState1.phasorMul   = 1.f + params[pi++] * 3.f;
            // params 12-13 reserved
        }

        // params 15-21 → FM2 (pitch)
        {
            size_t pi = 15;
            fmState2.carrierFreq = 1.f + params[pi++] * 3.f;
            fmState2.modFreq     = 1.f + params[pi++] * 7.f;
            fmState2.modIndex    = params[pi++] * 4.f;
            fmState2.fbLevel     = params[pi++];
            fmState2.phasorMul   = 1.f + params[pi++] * 3.f;
            // params 20-21 reserved
        }

        // params 22-24 → sine shaper
        sineShapeGain = params[22];
        sineShapeASym = params[23] * 0.5f;
        sineShapeMix  = params[24] * 0.5f;  // dialled down 50%

        // params 25-29 → FM3 (bitcrush depth)
        {
            size_t pi = 25;
            fmState3.carrierFreq = 1.f + params[pi++] * 3.f;
            fmState3.modFreq     = 1.f + params[pi++] * 7.f;
            fmState3.modIndex    = params[pi++] * 4.f;
            fmState3.fbLevel     = params[pi++];
            fmState3.phasorMul   = 1.f + params[pi++] * 3.f;
        }
    }

    void setTimeSignature(float beats, float division) {
        timeSigBeats = beats;
        seqEngine.setTimeSignature(beats, division);
        updatePlaybackRate();
    }

protected:
    void updatePlaybackRate() {
        if (loadedSampleCount == 0.f || sampleRatef == 0.f) return;
        float barLengthSamples = (60.f / currentBPM) * timeSigBeats * sampleRatef;
        flashPlaybackRate  = loadedSampleCount / barLengthSamples;
        currentPlaybackRate = flashPlaybackRate * pitchFactor;
    }

    float sampleRatef        = 48000.f;
    float currentBPM         = 90.f;
    float timeSigBeats       = 4.f;
    float loadedSampleCount  = 0.f;

    FMPatternGenState fmState1;
    FMPatternGenState fmState2;
    FMPatternGenState fmState3;
    BreakbeatPlayer   beatPlayer;
    float bitCrushLevels      = 65536.f;  // 16 bits — transparent until first trigger
    float fmValue            = 0.f;
    float pitchFactor        = 1.f;
    float flashPlaybackRate  = 1.f;
    float currentPlaybackRate = 1.f;
    bool  prevTrig           = false;

    float sineShapeGain      = 0.1f;
    float sineShapeASym      = 0.f;
    float sineShapeMix       = 0.f;
};

#endif  // __BRAYKORE_AUDIO_APP_HPP__
