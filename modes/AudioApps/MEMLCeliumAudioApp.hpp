#ifndef __MEMLCELIUM_AUDIO_APP_HPP__
#define __MEMLCELIUM_AUDIO_APP_HPP__

#include "../../src/memllib/audio/AudioAppBase.hpp"
#include "../../src/memllib/synth/maximilian.h"

#include <cstddef>
#include <cstdint>
#include <memory>

#include "../../src/memllib/synth/maxiPAF.hpp"
#include "../../src/memllib/synth/ADSRLite.hpp"
#include "../../src/memllib/interface/InterfaceBase.hpp"

#include <span>

#include "../../voicespaces/VoiceSpaces.hpp"

#include "RatioSeqEngine.hpp"

static constexpr size_t kMEMLCeliumNSequences = 3;

// MIDI notes assigned to each sequencer index
// static constexpr uint8_t kMEMLCeliumSeqNotes[kMEMLCeliumNSequences] = {60};

template<size_t NPARAMS=83>
class MEMLCeliumAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nFREQs = 17;
    static constexpr float frequencies[nFREQs] = {100, 200, 400,800, 400, 800, 100,1600,100,400,100,50,1600,200,100,800,400};
    static constexpr size_t nVoiceSpaces=7;

    // Focus group bitmasks
    static constexpr uint32_t kFocusSeq = (1u << 0);
    static constexpr uint32_t kFocusSyn = (1u << 1);
    static constexpr uint32_t kFocusEnv = (1u << 2);
    static constexpr uint32_t kFocusV0  = (1u << 3);
    static constexpr uint32_t kFocusV1  = (1u << 4);
    static constexpr uint32_t kFocusV2  = (1u << 5);

    // Per-param group membership
    static constexpr std::array<uint32_t, NPARAMS> kParamGroupMask = {
        // 0-6: seq V0 (7)
        kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0,
        kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0,
        // 7-13: seq V1 (7)
        kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1,
        kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1,
        // 14-20: seq V2 (7)
        kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2,
        kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2,
        // 21-32: V0 spectral — baseFreq, cf×3, bw×3, vib, vfr, shift×3 (12)
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        // 33-39: V0 envelopes — v0AmpEnv ADSR, v0PitchEnv AD, v0PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0,
        // 40-43: V0 shape/effects — sineShape×3, rmGain (4)
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        // 44-55: V1 spectral — v1BaseFreq, detune×2, cf×3, bw×3, shift×3 (12)
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        // 56-62: V1 envelopes — v1AmpEnv ADSR, v1PitchEnv AD, v1PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1,
        // 63-74: V2 spectral — v2BaseFreq, detune×2, cf×3, bw×3, shift×3 (12)
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        // 75-81: V2 envelopes — v2AmpEnv ADSR, v2PitchEnv AD, v2PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2,
        // 82: V2 ring mod (1)
        kFocusSyn|kFocusV2,
    };

    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;

    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    RatioSeqEngine<kMEMLCeliumNSequences> seqEngine;

    queue_t sequencerControlQueue;
    queue_t bpmControlQueue;

    std::array<String, nVoiceSpaces> getVoiceSpaceNames() {
        std::array<String, nVoiceSpaces> names;
        for(size_t i=0; i < voiceSpaces.size(); i++) {
            names[i] = voiceSpaces[i].name;
        }
        return names;
    }

    void setVoiceSpace(size_t i) {
        if (i < voiceSpaces.size() && voiceSpaces[i].mappingFunction) {
            currentVoiceSpace = voiceSpaces[i].mappingFunction;
        }
    }

    size_t getPopulatedVoiceSpaceCount() const {
        size_t count = 0;
        for (const auto& vs : voiceSpaces) {
            if (vs.mappingFunction) count++;
        }
        return count;
    }

    MEMLCeliumAudioApp() : AudioAppBase<NPARAMS>() {
        queue_init(&sequencerControlQueue, sizeof(int), 1);
        queue_init(&bpmControlQueue, sizeof(float), 1);
        queue_init(&qMIDINoteOn, sizeof(uint8_t)*2, 1);
        queue_init(&qMIDINoteOff, sizeof(uint8_t)*2, 1);
    };

    bool __force_inline euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
    {
        const float fi = phase * n;
        int i = static_cast<int>(fi);
        const float rem = fi - i;
        if (i == n)
        {
            i--;
        }
        const int idx = ((i + n - offset) * k) % n;
        return (idx < k && rem < pulseWidth) ? 1 : 0;
    }

    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        seqEngine.tick();

        float x1[1];

        float envval = v0AmpEnv.play();
        float v0PitchEnvVal = v0PitchEnv.play() * v0PitchEmph;

        float fbsmooth = (fbzm1 * fbSmoothAlpha) + (feedback * (1.f-fbSmoothAlpha));
        fbzm1 = fbsmooth;

        float freq0 = baseFreq * (1.f +  fbsmooth) + (v0PitchEnvVal * baseFreq);
        paf0.play(x1, 1, freq0, freq0 + (paf0_cf * freq0), paf0_bw, paf0_vib, paf0_vfr, paf0_shift, 0);
        float p0 = *x1 * p0Gain;

        const float freq1 = freq0 * detune1;

        paf1.play(x1, 1, freq1, freq1 + (paf1_cf * freq1), paf1_bw, paf1_vib, paf1_vfr, paf1_shift, 1);
        const float p1 = *x1 * p1Gain;

        const float freq2 = freq1 * detune2;

        paf2.play(x1, 1, freq2, freq2 + (paf2_cf * freq2), paf2_bw, paf2_vib, paf2_vfr, paf2_shift, 1);
        const float p2 = *x1 * p2Gain;

        float v0 = p0 + p1 + p2;// + p3;



        v0 = v0 * envval;
        feedback = v0 * feedbackGain;

        //v1
        float v1Envval = v1AmpEnv.play();
        float v1PitchEnvVal = v1PitchEnv.play() * v1PitchEmph;

        float v1freq0 = v1BaseFreq + (v1PitchEnvVal * v1BaseFreq);
        v1paf0.play(x1, 1, v1freq0, v1freq0 + (v1paf0_cf * v1freq0), v1paf0_bw, 0, 0, v1paf0_shift, 0);
        float v1p0 = *x1 * v1p0Gain;

        const float v1freq1 = v1freq0 * v1Detune1;

        v1paf1.play(x1, 1, v1freq1, v1freq1 + (v1paf1_cf * v1freq1), v1paf1_bw, 0, 0, v1paf1_shift, 1);
        const float v1p1 = *x1 * v1p1Gain;

        const float v1freq2 = v1freq1 * v1Detune2;

        v1paf2.play(x1, 1, v1freq2, v1freq2 + (v1paf2_cf * freq2), v1paf2_bw, 0, 0, v1paf2_shift, 1);
        const float v1p2 = *x1 * v1p2Gain;

        float v1 = v1p0 + v1p1 + v1p2;

        const float rm = v1p0 * v1p1 * v1p2;// * p3;

        v1 = ((1.0 - rmGain) * v1) + (rm * rmGain);

        v1 = v1 * v1Envval;

        //v2 — hihat-biased PAF voice
        float v2Envval = v2AmpEnv.play();
        float v2PitchEnvVal = v2PitchEnv.play() * v2PitchEmph;

        float v2freq0 = v2BaseFreq + (v2PitchEnvVal * v2BaseFreq);
        v2paf0.play(x1, 1, v2freq0, v2freq0 + (v2paf0_cf * v2freq0), v2paf0_bw, 0, 0, v2paf0_shift, 0);
        float v2p0 = *x1 * v2p0Gain;

        const float v2freq1 = v2freq0 * v2Detune1;
        v2paf1.play(x1, 1, v2freq1, v2freq1 + (v2paf1_cf * v2freq1), v2paf1_bw, 0, 0, v2paf1_shift, 1);
        const float v2p1 = *x1 * v2p1Gain;

        const float v2freq2 = v2freq1 * v2Detune2;
        v2paf2.play(x1, 1, v2freq2, v2freq2 + (v2paf2_cf * v2freq2), v2paf2_bw, 0, 0, v2paf2_shift, 1);
        const float v2p2 = *x1 * v2p2Gain;

        float v2 = v2p0 + v2p1 + v2p2;
        const float v2rm = v2p0 * v2p1 * v2p2;
        v2 = ((1.f - v2rmGain) * v2) + (v2rm * v2rmGain);
        v2 = v2 * v2Envval;

        float mix = v0 + v1 + v2;

        float shape = sinf(mix * TWOPI);
        shape = sinf(((shape * TWOPI) * sineShapeGain) + sineShapeASym);
        mix = mix + (shape * sineShapeMix);

        mix = lowBoost.play(mix);
        mix = midBoost.play(mix);    
        mix = highBoost.play(mix);

        
        mix = tanhf(mix);

        stereosample_t ret { mix, mix };
        return ret;
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override
    {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        maxiSettings::sampleRate = sample_rate;
        sampleRatef = static_cast<float>(sample_rate);

        paf0.init();
        paf0.setsr(maxiSettings::getSampleRate(), 1);

        paf1.init();
        paf1.setsr(maxiSettings::getSampleRate(), 1);

        paf2.init();
        paf2.setsr(maxiSettings::getSampleRate(), 1);

        paf3.init();
        paf3.setsr(maxiSettings::getSampleRate(), 1);

        v1paf0.init();
        v1paf0.setsr(maxiSettings::getSampleRate(), 1);

        v1paf1.init();
        v1paf1.setsr(maxiSettings::getSampleRate(), 1);

        v1paf2.init();
        v1paf2.setsr(maxiSettings::getSampleRate(), 1);

        v2paf0.init();
        v2paf0.setsr(maxiSettings::getSampleRate(), 1);

        v2paf1.init();
        v2paf1.setsr(maxiSettings::getSampleRate(), 1);

        v2paf2.init();
        v2paf2.setsr(maxiSettings::getSampleRate(), 1);

        arpFreq = frequencies[0];
        envamp=1.f;

        v0AmpEnv.setup(500,500,0.8,1000,sampleRatef);
        v0PitchEnv.setup(10,500,0.f,100,sampleRatef);
        v1AmpEnv.setup(500,500,0.8,1000,sampleRatef);
        v1PitchEnv.setup(10,500,0.f,100,sampleRatef);
        v2AmpEnv.setup(1,50,0.f,20,sampleRatef);
        v2PitchEnv.setup(1,30,0.f,10,sampleRatef);

        lowBoost.set(maxiBiquad::PEAK, 70.f, 0.707f, 6.f);
        midBoost.set(maxiBiquad::PEAK, 800.f, 0.707f, 6.f);
        highBoost.set(maxiBiquad::PEAK, 5000.f, 0.707f, 6.f);

        seqEngine.setup(sample_rate);
        seqEngine.updateBPM(120.f);
        seqEngine.setPlaying(true);

        seqEngine.onNoteOn = [this](size_t seqIdx, int velocity) {
            // uint8_t note = kMEMLCeliumSeqNotes[seqIdx];
            // uint8_t midimsg[2] = { note, static_cast<uint8_t>(velocity) };
            // queue_try_add(&qMIDINoteOn, &midimsg);
            // baseFreq = 80.f;
            noteVel = velocity / 127.0f;
            noteVel = noteVel * noteVel;
            switch(seqIdx) {
                case 0:
                    v0AmpEnv.trigger(noteVel);
                    v0PitchEnv.trigger(1.0);                    
                    break;
                case 1:
                    v1AmpEnv.trigger(noteVel);
                    v1PitchEnv.trigger(1.0);
                    break;
                case 2:
                    v2AmpEnv.trigger(noteVel);
                    v2PitchEnv.trigger(1.0);
                    break;
            }
        };
        seqEngine.onNoteOff = [this](size_t seqIdx) {
            // uint8_t note = kMEMLCeliumSeqNotes[seqIdx];
            // uint8_t midimsg[2] = { note, 0 };
            // queue_try_add(&qMIDINoteOff, &midimsg);
            switch(seqIdx) {
                case 0:
                    v0AmpEnv.release();
                    v0PitchEnv.release();
                    break;
                case 1:
                    v1AmpEnv.release();
                    v1PitchEnv.release();
                    break;
                case 2:
                    v2AmpEnv.release();
                    v2PitchEnv.release();
                    break;
            }
        };

        voiceSpaces[0] = {"Default", [this](const std::array<float, NPARAMS>& params) {
            seqEngine.updateParams(params, 0);

            size_t paramIdx = 21;
            auto sqParam = [&]() { const float p = params[paramIdx++]; return p * p; };

            baseFreq = 60.f + (params[paramIdx++] * 10.f);

            paf0_cf = (params[paramIdx++]  * 2.f);
            paf1_cf = (params[paramIdx++]  * 2.f);
            paf2_cf = (params[paramIdx++] * 2.f);

            paf0_bw = 10.f + (params[paramIdx++] * 100.f);
            paf1_bw = 10.f + (params[paramIdx++] * 100.f);
            paf2_bw = 10.f + (params[paramIdx++] * 100.f);

            paf0_vib = sqParam() * 0.01f;
            paf1_vib = paf0_vib;
            paf2_vib = paf0_vib;

            paf0_vfr = sqParam() * 15.0f;
            paf1_vfr = paf0_vfr;
            paf2_vfr = paf0_vfr;

            paf0_shift =  -100.f + (params[paramIdx++] * 200.f);
            paf1_shift = -100.f + (params[paramIdx++] * 200.f);
            paf2_shift = -100.f + (params[paramIdx++] * 200.f);

            v0AmpEnv.setup(0.01f + (params[paramIdx++] * 1.f),
                0.5f + sqParam() * 200.f,
                0.01 + (params[paramIdx++] * 0.5f), 1.f + sqParam() * 800.f, sampleRatef);

            v0PitchEnv.setup(0.01f + (params[paramIdx++] * 3.f),
                0.5f + sqParam() * 100.f,
                0.f, 0.1f, sampleRatef);

            v0PitchEmph = params[paramIdx++] * 50.f;

            sineShapeGain = params[paramIdx++];
            sineShapeASym = params[paramIdx++] * 0.5f;
            sineShapeMix = params[paramIdx++];

            rmGain = params[paramIdx++];
            feedbackGain = 0.f;
            fbSmoothAlpha = 0.5f;

            v1p0Gain = 1.f;
            v1p1Gain = 1.f;
            v1p2Gain = 1.f;

            v1BaseFreq = 300.f + (params[paramIdx++] * 10.f);
            v1Detune1 = 1.0f + (params[paramIdx++] * 1.0f);
            v1Detune2 = 1.0f + (params[paramIdx++] * 1.0f);

            v1paf0_cf = (params[paramIdx++] * 2.f);
            v1paf1_cf = (params[paramIdx++] * 2.f);
            v1paf2_cf = (params[paramIdx++] * 2.f);

            v1paf0_bw = 10.f + (params[paramIdx++] * 400.f);
            v1paf1_bw = 10.f + (params[paramIdx++] * 600.f);
            v1paf2_bw = 10.f + (params[paramIdx++] * 500.f);

            v1paf0_shift = -500.f + (params[paramIdx++] * 1000.f);
            v1paf1_shift = -300.f + (params[paramIdx++] * 600.f);
            v1paf2_shift = -100.f + (params[paramIdx++] * 200.f);

            v1AmpEnv.setup(0.01f + (params[paramIdx++] * 1.f),
                0.5f + sqParam() * 100.f,
                0.01 + (params[paramIdx++] * 0.3f), 1.f + sqParam() * 200.f, sampleRatef);

            v1PitchEnv.setup(0.01f + (params[paramIdx++] * 3.f),
                0.5f + sqParam() * 100.f,
                0.f, 0.1f, sampleRatef);

            v1PitchEmph = params[paramIdx++] * 10.f;

            v2p0Gain = 1.f;
            v2p1Gain = 1.f;
            v2p2Gain = 1.f;

            v2BaseFreq = 1000.f + (params[paramIdx++] * 7000.f);
            v2Detune1 = 1.0f + (params[paramIdx++] * 3.0f);
            v2Detune2 = 1.0f + (params[paramIdx++] * 3.0f);

            v2paf0_cf = params[paramIdx++] * 3.f;
            v2paf1_cf = params[paramIdx++] * 3.f;
            v2paf2_cf = params[paramIdx++] * 3.f;

            v2paf0_bw = 100.f + (params[paramIdx++] * 900.f);
            v2paf1_bw = 100.f + (params[paramIdx++] * 900.f);
            v2paf2_bw = 100.f + (params[paramIdx++] * 900.f);

            v2paf0_shift = -200.f + (params[paramIdx++] * 400.f);
            v2paf1_shift = -200.f + (params[paramIdx++] * 400.f);
            v2paf2_shift = -200.f + (params[paramIdx++] * 400.f);

            v2AmpEnv.setup(0.01f + (params[paramIdx++] * 0.1f),
                0.5f + sqParam() * 50.f,
                params[paramIdx++] * 0.1f, 1.f + sqParam() * 50.f, sampleRatef);

            v2PitchEnv.setup(0.01f + (params[paramIdx++] * 1.f),
                0.5f + sqParam() * 30.f,
                0.f, 0.1f, sampleRatef);

            v2PitchEmph = params[paramIdx++] * 5.f;
            v2rmGain = params[paramIdx++];
        }};
        currentVoiceSpace = voiceSpaces[0].mappingFunction;
    }

    inline float mtof(uint8_t note) {
        return 440.0f * exp2f((note - 69) / 12.0f);
    }

    size_t currNote=0;
    void loop() override {
        float newBPM;
        if (queue_try_remove(&bpmControlQueue, &newBPM)) {
            seqEngine.updateBPM(newBPM);
        }
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        firstParamsReceived = true;
        int seqControl;
        if (queue_try_remove(&sequencerControlQueue, &seqControl)) {
            seqEngine.setPlaying(seqControl == 1);
        }
        p0Gain=1.f;
        p1Gain=1.f;
        p2Gain=1.f;
        p3Gain=1.f;

        detune1 = 1.01f;
        detune2 = 1.02f;

        if (currentVoiceSpace) currentVoiceSpace(params);
    }

    queue_t qMIDINoteOn, qMIDINoteOff;



protected:

    maxiPAFOperator paf0;
    maxiPAFOperator paf1;
    maxiPAFOperator paf2;
    maxiPAFOperator paf3;

    maxiPAFOperator v1paf0;
    maxiPAFOperator v1paf1;
    maxiPAFOperator v1paf2;

    maxiPAFOperator v2paf0;
    maxiPAFOperator v2paf1;
    maxiPAFOperator v2paf2;

    maxiOsc pulse;

    ADSRLite v0AmpEnv, v0PitchEnv, v1AmpEnv, v1PitchEnv, v2AmpEnv, v2PitchEnv;

    float frame=0;

    float feedback=0.f, feedbackGain=0.f;

    float p0Gain=1.f, p1Gain = 1.f, p2Gain=1.f, p3Gain=1.f;
    float v1p0Gain=1.f, v1p1Gain=1.f, v1p2Gain=1.f;
    float v2p0Gain=1.f, v2p1Gain=1.f, v2p2Gain=1.f;

    float v0PitchEmph = 1.f;
    float v1PitchEmph = 1.f;
    float v2PitchEmph = 1.f;

    float paf0_freq = 100;
    float paf1_freq = 100;
    float paf2_freq = 50;
    float paf3_freq = 50;

    float paf0_cf = 200;
    float paf1_cf = 250;
    float paf2_cf = 250;
    float paf3_cf = 250;

    float v1paf0_cf = 200;
    float v1paf1_cf = 250;
    float v1paf2_cf = 250;

    float paf0_bw = 100;
    float paf1_bw = 5000;
    float paf2_bw = 5000;
    float paf3_bw = 5000;

    float v1paf0_bw = 100;
    float v1paf1_bw = 5000;
    float v1paf2_bw = 5000;

    float paf0_vib = 0;
    float paf1_vib = 1;
    float paf2_vib = 1;
    float paf3_vib = 1;

    float paf0_vfr = 2;
    float paf1_vfr = 2;
    float paf2_vfr = 2;
    float paf3_vfr = 2;

    float paf0_shift = 0;
    float paf1_shift = 0;
    float paf2_shift = 0;
    float paf3_shift = 0;

    float v1paf0_shift = 0;
    float v1paf1_shift = 0;
    float v1paf2_shift = 0;

    float v2paf0_cf = 1.f, v2paf1_cf = 1.5f, v2paf2_cf = 2.f;
    float v2paf0_bw = 500.f, v2paf1_bw = 800.f, v2paf2_bw = 600.f;
    float v2paf0_shift = 0.f, v2paf1_shift = 0.f, v2paf2_shift = 0.f;

    float rmGain = 0.f;
    float v2rmGain = 0.5f;

    float sineShapeGain=0.1;
    float sineShapeASym = 0.f;
    float sineShapeMix = 0.f;
    float sineShapeMixInv = 1.f;
    size_t counter=0;
    size_t freqIndex = 0;
    size_t freqOffset = 0;
    float arpFreq=50;

    maxiLine line;
    float envamp=0.f;

    float detune1 = 1.0;
    float detune2 = 1.0;
    float detune3 = 1.0;

    float v1Detune1 = 1.5;
    float v1Detune2 = 2.1;

    float v2BaseFreq = 3000.f;
    float v2Detune1 = 1.5f;
    float v2Detune2 = 2.1f;

    maxiOsc phasorOsc;
    maxiTrigger zxdetect;

    size_t euclidN=4;

    float baseFreq = 50.0f;
    float v1BaseFreq = 200.0f;
    bool newNote=false;
    float noteVel = 0.f;
    bool firstParamsReceived = false;

    // float envdec=0.2f/9000.f;

    float sampleRatef = maxiSettings::getSampleRate();

    float fbzm1=0.f;
    size_t delayMax=10;
    float fbSmoothAlpha=0.95f;

    maxiBiquad lowBoost, midBoost, highBoost;

};

#endif  // __MEMLCELIUM_AUDIO_APP_HPP__
