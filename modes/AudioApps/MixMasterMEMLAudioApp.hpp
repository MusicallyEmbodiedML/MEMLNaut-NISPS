#ifndef __MIXMASTERMEML_AUDIO_APP_HPP__
#define __MIXMASTERMEML_AUDIO_APP_HPP__

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

#include "GlitchSeqEngine.hpp"
#include "../../src/memllib/synth/GrainDelayI16.hpp"

static constexpr size_t kMixMasterMEMLNSequences = 4;  // 3 drum voices + bass

// MIDI notes assigned to each sequencer index
// static constexpr uint8_t kMixMasterMEMLSeqNotes[kMixMasterMEMLNSequences] = {60};

template<size_t NPARAMS=122>
class MixMasterMEMLAudioApp : public AudioAppBase<NPARAMS>
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
    static constexpr uint32_t kFocusFX  = (1u << 6);
    static constexpr uint32_t kFocusArp = (1u << 7);
    static constexpr uint32_t kFocusBass = (1u << 8);

    // Per-param group membership
    static constexpr std::array<uint32_t, NPARAMS> kParamGroupMask = {
        // Sequencer, 8 per voice: 3 ratios, speed, offset, 3 note levels (rest/low/full).
        // 0-7: seq V0
        kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0,
        kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0, kFocusSeq|kFocusV0,
        // 8-15: seq V1
        kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1,
        kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1, kFocusSeq|kFocusV1,
        // 16-23: seq V2
        kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2,
        kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2, kFocusSeq|kFocusV2,
        // 24-35: V0 spectral — baseFreq, cf×3, bw×3, vib, vfr, shift×3 (12)
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        // 36-42: V0 envelopes — v0AmpEnv ADSR, v0PitchEnv AD, v0PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0, kFocusSyn|kFocusEnv|kFocusV0,
        kFocusSyn|kFocusEnv|kFocusV0,
        // 43-46: V0 shape/effects — sineShape×3, rmGain (4)
        kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0, kFocusSyn|kFocusV0,
        // 47-58: V1 spectral — v1BaseFreq, detune×2, cf×3, bw×3, shift×3 (12)
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        kFocusSyn|kFocusV1, kFocusSyn|kFocusV1, kFocusSyn|kFocusV1,
        // 59-65: V1 envelopes — v1AmpEnv ADSR, v1PitchEnv AD, v1PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1, kFocusSyn|kFocusEnv|kFocusV1,
        kFocusSyn|kFocusEnv|kFocusV1,
        // 66-77: V2 spectral — v2BaseFreq, detune×2, cf×3, bw×3, shift×3 (12)
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        kFocusSyn|kFocusV2, kFocusSyn|kFocusV2, kFocusSyn|kFocusV2,
        // 78-84: V2 envelopes — v2AmpEnv ADSR, v2PitchEnv AD, v2PitchEmph (7)
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2, kFocusSyn|kFocusEnv|kFocusV2,
        kFocusSyn|kFocusEnv|kFocusV2,
        // 85: V2 ring mod (1)
        kFocusSyn|kFocusV2,
        // 86-91: master grain delay — length, time, feedback, pitch, spread, wet/dry (6)
        kFocusFX, kFocusFX, kFocusFX, kFocusFX, kFocusFX, kFocusFX,
        // 92-104: arp — step length, euclid hits, octaves, pattern, waveform, grain length,
        // euclid steps, pitch spread, pitch, attack, release, level, euclid rotation (13)
        kFocusArp, kFocusArp, kFocusArp, kFocusArp, kFocusArp, kFocusArp, kFocusArp,
        kFocusArp, kFocusArp, kFocusArp, kFocusArp, kFocusArp, kFocusArp,
        // 105-112: bass sequencer (as the drum voices) — 3 ratios, speed, offset, 3 levels
        kFocusSeq|kFocusBass, kFocusSeq|kFocusBass, kFocusSeq|kFocusBass, kFocusSeq|kFocusBass,
        kFocusSeq|kFocusBass, kFocusSeq|kFocusBass, kFocusSeq|kFocusBass, kFocusSeq|kFocusBass,
        // 113-119: bass sound — formant, bandwidth, attack, decay, sustain, release, level
        kFocusSyn|kFocusBass, kFocusSyn|kFocusBass,
        kFocusEnv|kFocusBass, kFocusEnv|kFocusBass, kFocusEnv|kFocusBass, kFocusEnv|kFocusBass,
        kFocusBass,
        // 120-121: bass line — slope, bend (4-note pitch curve)
        kFocusBass, kFocusBass,
    };

    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;

    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    GlitchSeqEngine<kMixMasterMEMLNSequences> seqEngine;

    queue_t sequencerControlQueue;
    queue_t bpmControlQueue;

    // Per-voice enable bits (bit 0 = V0, 1 = V1, 2 = V2). Set by the enable screen on the
    // control core, read on the audio core — all three on by default.
    // Enable bits (Enable screen; set on the control core, read on the audio core):
    // 0-2 voices V1-V3, 3 arp, 4 bass, 5 sine shaper, 6 grain delay (off = dry, buffer
    // still fed).
    static constexpr uint32_t kEnableArp = 1u << 3;
    static constexpr uint32_t kEnableBass = 1u << 4;
    static constexpr uint32_t kEnableShaper = 1u << 5;
    static constexpr uint32_t kEnableDelay = 1u << 6;
    volatile uint32_t voiceEnableMask_ = 0b1111111;

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

    // Codec output level up from the 0.55 default (as SaxFX/ChunkyBits): sparse, quiet
    // material (rests, 10% notes) otherwise comes out low.
    AudioDriver::codec_config_t GetDriverConfig() const override {
        return {
            .mic_input     = false,
            .line_level    = 3,
            .mic_gain_dB   = 0,
            .output_volume = 0.97f
        };
    }

    MixMasterMEMLAudioApp() : AudioAppBase<NPARAMS>() {
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

        // Arp clock: the sequencer's own bar count + phase, so steps and the Euclidean
        // pattern sit on the same grid as the drums and start with them.
        if (seqEngine.playing) {
            const float pos = static_cast<float>(seqEngine.getBarCount()) + seqEngine.getBarPhasor();
            const int step = static_cast<int>(pos * 4.f * arpStepsPerBeat_);
            if (step != arpLastStep_) {
                arpLastStep_ = step;
                // Euclidean: step pos (rotated) is a hit if pos*k mod n < k. Packed in one
                // word so the audio core never sees a half-updated n/k/rotation.
                const uint32_t e = arpEuclid_;
                const int n = static_cast<int>(e >> 16), k = static_cast<int>((e >> 8) & 0xFF);
                const int rot = static_cast<int>(e & 0xFF);
                const int p = ((step + rot) % n + n) % n;
                if ((p * k) % n < k) {
                    // Trigger here, on the audio core, so the note lands on the step. The
                    // next note is already loaded in the idle voice; switch to it and ask
                    // the control loop to load the one after. (If that isn't ready yet,
                    // retrigger the current voice rather than drop the hit.)
                    if (arpNextReady_) {
                        arpCur_ ^= 1;
                        arpNextReady_ = false;
                        arpPrepare_ = true;
                    }
                    arpEnvs_[arpCur_].trigger(1.f);
                }
            }
        } else {
            arpLastStep_ = -1;
        }
        const float arpOut = (arpVoices_[0].process(0.f) * arpEnvs_[0].play()
                            + arpVoices_[1].process(0.f) * arpEnvs_[1].play()) * arpLevel_;

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



        v0 = v0 * envval * kV0Gain * mixGain_[kMixV1];
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

        v1 = v1 * v1Envval * mixGain_[kMixV2];

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
        v2 = v2 * v2Envval * mixGain_[kMixV3];

        if (!(voiceEnableMask_ & (1u << 0))) v0 = 0.f;
        if (!(voiceEnableMask_ & (1u << 1))) v1 = 0.f;
        if (!(voiceEnableMask_ & (1u << 2))) v2 = 0.f;

        float mix = v0 + v1 + v2;

        if (voiceEnableMask_ & kEnableShaper) {
            float shape = sinf(mix * TWOPI);
            shape = sinf(((shape * TWOPI) * sineShapeGain) + sineShapeASym);
            mix = mix + (shape * sineShapeMix);
        }


        mix = lowBoost.play(mix);
        mix = midBoost.play(mix);    
        mix = highBoost.play(mix);
        if (voiceEnableMask_ & kEnableArp) mix += arpOut * mixGain_[kMixArp];  // into the grain delay with the rest

        
        // Master grain delay, wet/dry.
        const stereosample_t wet = masterGrain_.processStereo(mix);
        const float wetMix = (voiceEnableMask_ & kEnableDelay) ? grainMix_ : 0.f;
        const float dry = mix * (1.f - wetMix);
        // Bass: added after the delay (and so after the shaper and EQ), always dry: it
        // stays clean and at full level whatever the delay's wet/dry is doing.
        bassPaf_.play(x1, 1, bassFreq_, bassFreq_ * bassCf_, bassFreq_ * bassBw_, 0.f, 0.f, 0.f, false);
        const float bass = (voiceEnableMask_ & kEnableBass) ? *x1 * bassEnv_.play() * bassLevel_ * mixGain_[kMixBass] : (bassEnv_.play(), 0.f);

        // Mixer: the Delay fader trims the wet return only; Master drives the final tanh.
        const float wetGain = wetMix * mixGain_[kMixDelay];
        const float master = mixGain_[kMixMaster];
        stereosample_t ret { tanhf((dry + wet.L * wetGain + bass) * master),
                             tanhf((dry + wet.R * wetGain + bass) * master) };
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

        masterGrain_.setup(sample_rate);
        bassPaf_.init();
        bassPaf_.setsr(maxiSettings::getSampleRate(), 1);
        for (size_t i = 0; i < kNumBassNotes; i++) bassFreqs_[i] = mtof(kBassRoot + 12);  // C2 until params arrive
        bassFreq_ = bassFreqs_[0];
        bassEnv_.setup(20.f, 300.f, 0.4f, 300.f, sampleRatef);
        for (int v = 0; v < 2; v++) {
            arpVoices_[v].setup(sample_rate);
            arpVoices_[v].fillWithSaw(mtof(kArpRoot));
            // Frozen: the waveform cycle stays in the buffer, so a note sustains for its
            // whole envelope (unfrozen, the voice overwrites it within the delay time).
            arpVoices_[v].setFreeze(true);
            arpEnvs_[v].setup(20.f, 700.f, 0.f, 1.f, sampleRatef);
        }
        arpPrepare_ = true;  // load the first note into the idle voice

        seqEngine.setup(sample_rate);
        seqEngine.updateBPM(120.f);
        seqEngine.setPlaying(true);

        // level: the note's amplitude from its level param (low = 10%, full = 1).
        seqEngine.onNoteOn = [this](size_t seqIdx, float level) {
            noteVel = level;
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
                case 3:  // bass: next pitch from the list, in order
                    bassFreq_ = bassFreqs_[bassNoteIdx_];
                    bassNoteIdx_ = (bassNoteIdx_ + 1) % kNumBassNotes;
                    bassEnv_.trigger(noteVel);
                    break;
            }
        };
        seqEngine.onNoteOff = [this](size_t seqIdx) {
            // uint8_t note = kMixMasterMEMLSeqNotes[seqIdx];
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
                case 3:
                    bassEnv_.release();
                    break;
            }
        };

        voiceSpaces[0] = {"Default", [this](const std::array<float, NPARAMS>& params) {
            for (size_t v = 0; v < 3; v++)  // drum voices; the bass sequence is at the end
                seqEngine.updateSeqParams(v, params, v * decltype(seqEngine)::kParamsPerSeq);

            size_t paramIdx = 3 * decltype(seqEngine)::kParamsPerSeq;  // 24: after the sequencer
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

            // Sine shaper, gentler than MEMLCelium's (gain 0..1, mix 0..1) for ambient.
            sineShapeGain = params[paramIdx++] * 0.5f;
            sineShapeASym = params[paramIdx++] * 0.5f;
            sineShapeMix = params[paramIdx++] * 0.4f;

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

            // Master grain delay. Pitch snaps to musical ratios (octaves, fifths) so the
            // feedback builds shimmer rather than detuned mush; spread adds a slight detune.
            static constexpr float kGrainPitches[] = {0.5f, 0.75f, 1.f, 1.5f, 2.f};
            masterGrain_.setGrainLengthMs(20.f + sqParam() * 480.f);
            // Delay time: a beat division, synced to the sequencer tempo.
            static constexpr float kDelayBeats[] = {0.125f, 0.1875f, 0.25f, 0.375f, 0.5f,
                                                    0.75f, 1.f, 1.5f, 2.f};
            constexpr size_t kNDelayBeats = sizeof(kDelayBeats) / sizeof(kDelayBeats[0]);
            delayBeats_ = kDelayBeats[static_cast<int>(params[paramIdx++] * (kNDelayBeats - 0.001f))];
            applyDelayTime();
            masterGrain_.setFeedback(params[paramIdx++] * 0.85f);
            masterGrain_.setPitch(kGrainPitches[static_cast<int>(params[paramIdx++] * 4.999f)]);
            masterGrain_.setPitchSpread(sqParam() * 0.03f);
            grainMix_ = params[paramIdx++];

            // Arp: a ChunkyBits-style voice (a grain delay filled with a waveform cycle).
            // Step length, in beats: 1/64, 1/64T, 1/32, 1/32T, 1/16, 1/16T, 1/8, 1/8T, 1/4,
            // 1/4T, 1/2, 1/2T, 1 (whole).
            static constexpr float kArpStepBeats[] = {1.f / 16.f, 1.f / 24.f, 0.125f, 1.f / 12.f,
                                                      0.25f, 1.f / 6.f, 0.5f, 1.f / 3.f, 1.f,
                                                      2.f / 3.f, 2.f, 4.f / 3.f, 4.f};
            constexpr size_t kNStepLens = sizeof(kArpStepBeats) / sizeof(kArpStepBeats[0]);
            arpStepsPerBeat_ = 1.f / kArpStepBeats[static_cast<int>(params[paramIdx++] * (kNStepLens - 0.001f))];
            const float hitsP = params[paramIdx++];  // relative to the step count, set below
            arpOctaves_ = 1 + static_cast<int>(params[paramIdx++] * 2.999f);
            arpPattern_ = static_cast<int>(params[paramIdx++] * 3.999f);
            arpWave_ = static_cast<int>(params[paramIdx++] * (kNumArpWaves - 0.001f));
            const float grainLenMs = 10.f + params[paramIdx++] * 490.f;
            // Euclidean rhythm: k hits spread evenly over n steps, rotated.
            const int n = 2 + static_cast<int>(params[paramIdx++] * 14.999f);  // 2..16
            const int k = std::min(n, 1 + static_cast<int>(hitsP * n));        // 1..n
            const float spread = params[paramIdx++] * 0.3f;            // buffer is frozen: no feedback
            const float pitch = exp2f(params[paramIdx++] * 2.f - 1.f);  // +-1 octave
            const float attack = 5.f + sqParam() * 495.f;
            const float release = 50.f + sqParam() * 2950.f;
            for (int v = 0; v < 2; v++) {
                arpVoices_[v].setGrainLengthMs(grainLenMs);
                arpVoices_[v].setStartTimeMs(100.f);  // little effect with a frozen buffer
                arpVoices_[v].setPitchSpread(spread);
                arpVoices_[v].setPitch(pitch);
                arpEnvs_[v].setup(attack, release, 0.f, 1.f, sampleRatef);
            }
            arpLevel_ = 0.1f + params[paramIdx++] * 0.55f;  // ~0.03-0.2 RMS when active
            const int rot = std::min(n - 1, static_cast<int>(params[paramIdx++] * n));
            arpEuclid_ = (static_cast<uint32_t>(n) << 16) | (static_cast<uint32_t>(k) << 8) | static_cast<uint32_t>(rot);

            // Bass: rhythm and levels from sequence 3, exactly like the drum voices.
            seqEngine.updateSeqParams(3, params, paramIdx);
            paramIdx += decltype(seqEngine)::kParamsPerSeq;
            // One PAF operator. Formant centre and bandwidth scale with the note, so all
            // pitches share a timbre. Centre 2-6x the fundamental and bandwidth 1-5x keep it
            // harmonically rich: at 33-131Hz a near-sine is mostly inaudible on small
            // speakers and just fills the output saturation. Soft attack (no click),
            // decay/release from short to long.
            bassCf_ = 2.f + params[paramIdx++] * 4.f;
            bassBw_ = 1.f + sqParam() * 4.f;
            {
                const float attack = 5.f + sqParam() * 55.f;
                const float decay = 20.f + sqParam() * 1980.f;
                const float sustain = params[paramIdx++] * 0.8f;
                const float release = 20.f + sqParam() * 2980.f;
                bassEnv_.setup(attack, decay, sustain, release, sampleRatef);
            }
            bassLevel_ = 0.5f + params[paramIdx++] * 0.75f;  // ~0.08-0.2 RMS when active

            // Bass line: 4 notes from a curve f(x) = a*x + b*4x(1-x), sampled at x = 0, 1/3,
            // 2/3, 1. a = slope (falling..rising), b = bend (dip..arch). f is scaled to
            // scale steps around C2 and clamped to C1..C3; f(0) = 0, so the line starts on
            // C2. Pentatonic C D E G A: the same notes as the arp's A minor pentatonic.
            {
                const float a = params[paramIdx++] * 2.f - 1.f;
                const float b = params[paramIdx++] * 2.f - 1.f;
                for (size_t i = 0; i < kNumBassNotes; i++) {
                    const float x = static_cast<float>(i) / (kNumBassNotes - 1);
                    const float f = a * x + b * 4.f * x * (1.f - x);
                    const int deg = std::max(0, std::min(kBassMaxDeg,
                        kBassCentreDeg + static_cast<int>(lroundf(f * kBassStepsPerUnit))));
                    bassFreqs_[i] = mtof(kBassRoot + 12 * (deg / 5) + kBassScale[deg % 5]);
                }
            }
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
            bpm_ = newBPM;
            applyDelayTime();  // keep the grain delay on the beat
        }
        if (arpPrepare_) {  // set by Process(); refilling a buffer is too slow for there
            arpPrepare_ = false;
            arpPrepareNext();
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

    GrainDelayI16<65536, 4> masterGrain_;  // ~1.36s at 48kHz, 128KB
    float grainMix_ = 0.f;
    float bpm_ = 120.f;        // matches seqEngine's initial tempo
    float delayBeats_ = 0.5f;  // grain delay time as a beat division

    // Mix balance, set from measured RMS (typical active levels): voice 1 ~0.03-0.1,
    // arp ~0.03-0.2, bass ~0.08-0.2. Actual RMS also depends on density and envelopes.
    static constexpr float kV0Gain = 0.4f;  // voice 1 has no level param

public:
    // Mixer screen trims (x0..x2, unity 1), on top of the NN-set levels. Written by the
    // touch UI on core 0, read per sample here.
    enum MixChannel : size_t { kMixV1 = 0, kMixV2, kMixV3, kMixArp, kMixBass, kMixDelay, kMixMaster, kNumMix };
    volatile float mixGain_[kNumMix] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};
protected:

    // ─── Bass ─────────────────────────────────────────────────────────────────────
    // The bass line: kNumBassNotes pitches (set from the curve params), taken in turn.
    static constexpr size_t kNumBassNotes = 4;
    static constexpr uint8_t kBassRoot = 24;                    // C1
    static constexpr uint8_t kBassScale[5] = {0, 2, 4, 7, 9};   // C major pentatonic
    static constexpr int kBassCentreDeg = 5;                    // C2
    static constexpr int kBassMaxDeg = 10;                      // C3
    static constexpr float kBassStepsPerUnit = 4.f;
    maxiPAFOperator bassPaf_;
    ADSRLite bassEnv_;
    float bassFreqs_[kNumBassNotes] = {};
    float bassFreq_ = 32.7f;
    size_t bassNoteIdx_ = 0;
    float bassCf_ = 1.f;
    float bassBw_ = 1.f;
    float bassLevel_ = 0.f;

    // ─── Arp ──────────────────────────────────────────────────────────────────────
    // A ChunkyBits-style voice: a grain delay whose (frozen) buffer holds one waveform
    // cycle at the note's pitch; the grains read it back as a sustained, grainy tone.
    static constexpr uint8_t kArpRoot = 57;  // A3
    static constexpr uint8_t kArpScale[5] = {0, 3, 5, 7, 10};  // minor pentatonic
    static constexpr int kNumArpWaves = 9;
    // Two voices, alternating: one plays while the next note is loaded into the other.
    GrainDelayI16<16384, 2> arpVoices_[2];  // 32KB each
    ADSRLite arpEnvs_[2];
    volatile int arpCur_ = 0;             // voice of the note now playing
    volatile bool arpNextReady_ = false;  // idle voice holds the next note
    volatile bool arpPrepare_ = false;    // ask the control loop to load it
    float arpLevel_ = 0.f;
    float arpStepsPerBeat_ = 1.f;
    volatile uint32_t arpEuclid_ = (8u << 16) | (3u << 8);  // n=8, k=3, rotation 0
    int arpOctaves_ = 1;
    int arpPattern_ = 0;  // up, down, up-down, skip (by 2)
    int arpWave_ = 0;
    int arpPos_ = -1;
    int arpDir_ = 1;
    int arpLastStep_ = -1;

    // Control loop, not the audio path: pick the next note and load it into the idle
    // voice, ready for Process() to trigger on the next hit. Fully deterministic.
    void arpPrepareNext() {
        const int n = 5 * arpOctaves_;
        switch (arpPattern_) {
            case 0: arpPos_ = (arpPos_ + 1) % n; break;
            case 1: arpPos_ = (arpPos_ - 1 + 2 * n) % n; break;
            case 2:
                if (arpPos_ < 0) arpPos_ = 0;
                else {
                    if (arpPos_ + arpDir_ >= n || arpPos_ + arpDir_ < 0) arpDir_ = -arpDir_;
                    arpPos_ = std::max(0, std::min(n - 1, arpPos_ + arpDir_));
                }
                break;
            default: arpPos_ = (arpPos_ + 2) % n; break;  // skip: every other note, wrapping
        }
        const uint8_t note = kArpRoot + 12 * (arpPos_ / 5) + kArpScale[arpPos_ % 5];
        const float f = mtof(note);
        auto& idle = arpVoices_[arpCur_ ^ 1];
        switch (arpWave_) {
            case 0: idle.fillWithSaw(f); break;
            case 1: idle.fillWithSquare(f); break;
            case 2: idle.fillWithTriangle(f); break;
            case 3: idle.fillWithFallingSaw(f); break;
            case 4: idle.fillWithSine(f); break;
            case 5: idle.fillWithHalfRectSine(f); break;
            case 6: idle.fillWithTrapezoid(f); break;
            case 7: idle.fillWithAsymTriangle(f); break;
            default: idle.fillWithStaircase(f); break;
        }
        arpNextReady_ = true;
    }

    // Delay = delayBeats_ at the current tempo. Halved (staying on the beat grid) until
    // it fits the buffer, which matters at slow tempos.
    void applyDelayTime() {
        static constexpr float kMaxDelayMs = 1300.f;
        float ms = delayBeats_ * 60000.f / bpm_;
        while (ms > kMaxDelayMs) ms *= 0.5f;
        masterGrain_.setStartTimeMs(ms);
    }

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

#endif  // __MIXMASTERMEML_AUDIO_APP_HPP__
