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

static constexpr size_t kMEMLCeliumNSequences = 2;

// MIDI notes assigned to each sequencer index
// static constexpr uint8_t kMEMLCeliumSeqNotes[kMEMLCeliumNSequences] = {60};

template<size_t NPARAMS=64>
class MEMLCeliumAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nFREQs = 17;
    static constexpr float frequencies[nFREQs] = {100, 200, 400,800, 400, 800, 100,1600,100,400,100,50,1600,200,100,800,400};
    static constexpr size_t nVoiceSpaces=7;

    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;

    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    RatioSeqEngine<kMEMLCeliumNSequences> seqEngine;

    queue_t sequencerControlQueue;

    std::array<String, nVoiceSpaces> getVoiceSpaceNames() {
        std::array<String, nVoiceSpaces> names;
        for(size_t i=0; i < voiceSpaces.size(); i++) {
            names[i] = voiceSpaces[i].name;
        }
        return names;
    }

    void setVoiceSpace(size_t i) {
        if (i < voiceSpaces.size()) {
            currentVoiceSpace = voiceSpaces[i].mappingFunction;
        }
    }

    MEMLCeliumAudioApp() : AudioAppBase<NPARAMS>() {
        // auto voiceSpace1 = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_1_BODY
        // };

        // auto voiceSpace2 = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_2_BODY
        // };

        // auto voiceSpacePerc = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_PERC_BODY
        // };

        // auto voiceSpaceSingle1 = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_SINGLE_1_BODY
        // };

        // auto voiceSpaceQuadDetune = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_QUAD_DETUNE_BODY
        // };

        // auto voiceSpaceQuadOct = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_QUAD_OCT_BODY
        // };

        // auto voiceSpaceQuadDist = [this](const std::array<float, NPARAMS>& params) {
        //     VOICE_SPACE_QUAD_DIST_BODY
        // };


        // voiceSpaces[0] = {"Ellipticacacia", voiceSpaceQuadDetune};
        // voiceSpaces[1] = {"Rowantares", voiceSpace1};
        // voiceSpaces[2] = {"Neemeda", voiceSpace2};
        // voiceSpaces[3] = {"Aquillow", voiceSpacePerc};
        // voiceSpaces[4] = {"Magnetarch", voiceSpaceSingle1};
        // voiceSpaces[5] = {"Elderstar", voiceSpaceQuadOct};
        // voiceSpaces[6] = {"Ipeleiades", voiceSpaceQuadDist};

        // currentVoiceSpace = voiceSpaces[0].mappingFunction;

        queue_init(&sequencerControlQueue, sizeof(int), 1);
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

        const float rm = p0 * p1 * p2;// * p3;

        v0 = v0 + (rm * rmGain);


        float shape = sinf(v0 * TWOPI);
        shape = sinf(((shape * TWOPI) * sineShapeGain) + sineShapeASym);
        v0 = v0 + (shape * sineShapeMix);

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

        v1 = v1 * v1Envval;
        /////

        float mix = v0 + v1;

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

        arpFreq = frequencies[0];
        envamp=1.f;

        v0AmpEnv.setup(500,500,0.8,1000,sampleRatef);
        v0PitchEnv.setup(10,500,0.f,100,sampleRatef);
        v1AmpEnv.setup(500,500,0.8,1000,sampleRatef);
        v1PitchEnv.setup(10,500,0.f,100,sampleRatef);

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
                    break;
                case 1:
                    v1AmpEnv.trigger(noteVel);
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
                    break;
                case 1:
                    v1AmpEnv.release();
                    break;
            }
        };
    }

    inline float mtof(uint8_t note) {
        return 440.0f * exp2f((note - 69) / 12.0f);
    }

    size_t currNote=0;
    void loop() override {
        // uint8_t midimsg[2];
        // if (firstParamsReceived && queue_try_remove(&qMIDINoteOn, &midimsg)) {
        //     baseFreq = 80.f;
        //     noteVel = midimsg[1] / 127.0f;
        //     noteVel = noteVel * noteVel;
        //     newNote = true;
        //     env.trigger(noteVel);
        //     currNote = midimsg[0];
        // }
        // if (firstParamsReceived && queue_try_remove(&qMIDINoteOff, &midimsg)) {
        //     if (currNote == midimsg[0]) {
        //         env.release();
        //     }
        // }
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        firstParamsReceived = true;
        int seqControl;
        if (queue_try_remove(&sequencerControlQueue, &seqControl)) {
            seqEngine.setPlaying(seqControl == 1);
        }
        seqEngine.updateParams(params, 0);
        p0Gain=1.f; 
        p1Gain=1.f; 
        p2Gain=1.f; 
        p3Gain=1.f; 
        
        detune1 = 1.0f; 
        detune2 = 1.1f; 
        // detune3 = 1.2f; 

        size_t paramIdx = 14;  // 0-13 reserved for seqEngine (2 sequencers × 7 params)

        baseFreq = 20.f + (params[paramIdx++] * 80.f);
        
        paf0_cf = (params[paramIdx++]  * 2.f); 
        paf1_cf = (params[paramIdx++]  * 2.f); 
        paf2_cf = (params[paramIdx++] * 2.f); 
        // paf3_cf = (params[paramIdx++] * 2.f); 
        
        paf0_bw = 10.f + (params[paramIdx++] * 400.f); 
        paf1_bw = 10.f + (params[paramIdx++] * 50.f); 
        paf2_bw = 10.f + (params[paramIdx++] * 50.f); 
        // paf3_bw = 10.f + (params[paramIdx++] * 100.f); 
                
        paf0_vib = params[paramIdx++] * params[paramIdx++] * 0.01f; 
        paf1_vib = paf0_vib; 
        paf2_vib = paf0_vib; 
        // paf3_vib = 0.f; 
        
        paf0_vfr = (params[paramIdx++] * params[paramIdx++]* 15.0f);
        paf1_vfr = paf0_vfr;
        paf2_vfr = paf0_vfr; 
        // paf3_vfr = 0.f; 
        
        paf0_shift =  -500.f + (params[paramIdx++] * 500.f); 
        paf1_shift = -300.f + (params[paramIdx++] * 600.f); 
        paf2_shift = -100.f + (params[paramIdx++] * 200.f); 
        // paf3_shift = -300.f + (params[paramIdx++] * 300.f); 
                
        v0AmpEnv.setup(0.01f + (params[paramIdx++] * 1.f),
            0.5f + params[paramIdx++] * params[paramIdx++] * 200.f,
            0.01 + (params[paramIdx++] * 0.5f),1.f+ params[paramIdx++] * params[paramIdx++] * 800.f, sampleRatef ); 

        v0PitchEnv.setup(0.01f + (params[paramIdx++] * 3.f), 
            0.5f + params[paramIdx++] * params[paramIdx++] * 100.f,
            0.f, 0.1f, sampleRatef ); 

        v0PitchEmph = params[paramIdx++]* 50.f;
        
        sineShapeGain = params[paramIdx++]; 
        sineShapeASym = params[paramIdx++]* 0.5f; 
        sineShapeMix = params[paramIdx++]; 
        
        rmGain = params[paramIdx++];
        feedbackGain = 0.1f;

        fbSmoothAlpha=0.5f;

        //v1
        v1p0Gain=1.f;
        v1p1Gain=1.f;
        v1p2Gain=1.f;

        v1BaseFreq = 100.f + (params[paramIdx++] * 400.f);
        v1Detune1 = 0.5f + (params[paramIdx++] * 3.5f);
        v1Detune2 = 0.5f + (params[paramIdx++] * 3.5f);

        v1paf0_cf = (params[paramIdx++] * 3.f);
        v1paf1_cf = (params[paramIdx++] * 3.f);
        v1paf2_cf = (params[paramIdx++] * 3.f);

        v1paf0_bw = 10.f + (params[paramIdx++] * 400.f);
        v1paf1_bw = 10.f + (params[paramIdx++] * 600.f);
        v1paf2_bw = 10.f + (params[paramIdx++] * 500.f);

        v1paf0_shift = -800.f + (params[paramIdx++] * 800.f);
        v1paf1_shift = -300.f + (params[paramIdx++] * 600.f);
        v1paf2_shift = -100.f + (params[paramIdx++] * 200.f);

        v1AmpEnv.setup(0.01f + (params[paramIdx++] * 1.f),
            0.5f + params[paramIdx++] * params[paramIdx++] * 100.f,
            0.01 + (params[paramIdx++] * 0.3f), 1.f + params[paramIdx++] * params[paramIdx++] * 200.f, sampleRatef);

        v1PitchEnv.setup(0.01f + (params[paramIdx++] * 3.f),
            0.5f + params[paramIdx++] * params[paramIdx++] * 100.f,
            0.f, 0.1f, sampleRatef);

        v1PitchEmph = params[paramIdx++] * 10.f;

        // currentVoiceSpace(params);
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


    maxiOsc pulse;

    ADSRLite v0AmpEnv, v0PitchEnv, v1AmpEnv, v1PitchEnv;

    float frame=0;

    float feedback=0.f, feedbackGain=0.f;

    float p0Gain=1.f, p1Gain = 1.f, p2Gain=1.f, p3Gain=1.f;
    float v1p0Gain=1.f, v1p1Gain=1.f, v1p2Gain=1.f;

    float v0PitchEmph = 1.f;
    float v1PitchEmph = 1.f;

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


    float rmGain = 0.f;

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

};

#endif  // __MEMLCELIUM_AUDIO_APP_HPP__
