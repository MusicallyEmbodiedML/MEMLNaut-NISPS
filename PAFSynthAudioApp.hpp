#ifndef __PAF_SYNTH_AUDIO_APP_HPP__
#define __PAF_SYNTH_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp" // Added missing include
#include "src/memllib/synth/maximilian.h" // Added missing include for maxiSettings, maxiOsc, maxiTrigger, maxiDelayline, maxiEnvGen, maxiLine

#include <cstddef>
#include <cstdint>
#include <memory> // Added for std::shared_ptr

#include "src/memllib/synth/maxiPAF.hpp"
#include "src/memllib/synth/ADSRLite.hpp"
#include "src/memllib/interface/InterfaceBase.hpp" // Added missing include

#include <span>

#include "voicespaces/VoiceSpace1.hpp"
#include "voicespaces/VoiceSpace2.hpp"
#include "voicespaces/VoiceSpacePerc.hpp"
#include "voicespaces/VoiceSpaceSingle1.hpp"
#include "voicespaces/VoiceSpaceQuadDetune.hpp"
#include "voicespaces/VoiceSpaceQuadOct.hpp"
#include "voicespaces/VoiceSpaceQuadDist.hpp"



// #define ARPEGGIATOR

template<size_t NPARAMS=33>
// template<size_t NPARAMS=1>
class PAFSynthAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nFREQs = 17;
    static constexpr float frequencies[nFREQs] = {100, 200, 400,800, 400, 800, 100,1600,100,400,100,50,1600,200,100,800,400};
    static constexpr size_t nVoiceSpaces=7;

    using VoiceSpaceFn = std::function<void(const std::array<float, NPARAMS>&)>;
    struct VoiceSpace {
        char name[16]="default";
        VoiceSpaceFn mappingFunction = nullptr;
    };

    std::array<VoiceSpace, nVoiceSpaces> voiceSpaces;
    
    VoiceSpaceFn currentVoiceSpace;

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

    PAFSynthAudioApp() : AudioAppBase<NPARAMS>() {
        auto voiceSpace1 = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_1_BODY
        };

        auto voiceSpace2 = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_2_BODY
        };

        auto voiceSpacePerc = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_PERC_BODY
        };

        auto voiceSpaceSingle1 = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_SINGLE_1_BODY
        };

        auto voiceSpaceQuadDetune = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_QUAD_DETUNE_BODY
        };

        auto voiceSpaceQuadOct = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_QUAD_OCT_BODY
        };

        auto voiceSpaceQuadDist = [this](const std::array<float, NPARAMS>& params) {
            VOICE_SPACE_QUAD_DIST_BODY
        };
        

        voiceSpaces[0] = {"Ellipticacacia", voiceSpaceQuadDetune};
        voiceSpaces[1] = {"Rowantares", voiceSpace1};
        voiceSpaces[2] = {"Neemeda", voiceSpace2};
        voiceSpaces[3] = {"Aquillow", voiceSpacePerc};
        voiceSpaces[4] = {"Magnetarch", voiceSpaceSingle1};
        voiceSpaces[5] = {"Elderstar", voiceSpaceQuadOct};
        voiceSpaces[6] = {"Ipeleiades", voiceSpaceQuadDist};

        currentVoiceSpace = voiceSpaces[0].mappingFunction;   

    };

    bool __force_inline euclidean(float phase, const size_t n, const size_t k, const size_t offset, const float pulseWidth)
    {
        // Euclidean function
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

    // maxiOsc testosc;
    stereosample_t __force_inline Process(const stereosample_t x) override
    {
        float x1[1];

        // float p0 = testosc.sinewave(baseFreq);
        float fbsmooth = (fbzm1 * fbSmoothAlpha) + (feedback * (1.f-fbSmoothAlpha));
        fbzm1 = fbsmooth;

        float freq0 = baseFreq * (1.f +  fbsmooth);
        paf0.play(x1, 1, freq0, freq0 + (paf0_cf * freq0), paf0_bw, paf0_vib, paf0_vfr, paf0_shift, 0);
        float p0 = *x1 * p0Gain;

        const float freq1 = freq0 * detune1;

        paf1.play(x1, 1, freq1, freq1 + (paf1_cf * freq1), paf1_bw, paf1_vib, paf1_vfr, paf1_shift, 1);
        const float p1 = *x1 * p1Gain;

        const float freq2 = freq1 * detune2;

        paf2.play(x1, 1, freq2, freq2 + (paf2_cf * freq2), paf2_bw, paf2_vib, paf2_vfr, paf2_shift, 1);
        const float p2 = *x1 * p2Gain;

        const float freq3 = freq2 * detune3;

        paf3.play(x1, 1, freq3, freq3 + (paf3_cf * freq3), paf3_bw * freq3, paf3_vib, paf3_vfr, paf3_shift, 1);
        const float p3 = *x1 * p3Gain;

        float y = p0 + p1 + p2 + p3;
    
        const float rm = p0 * p1 * p2 * p3;
// 
        y = y + (rm * rmGain);

        float shape = sinf(y * TWOPI);
        shape = sinf(((shape * TWOPI) * sineShapeGain) + sineShapeASym);
        y = y + (shape * sineShapeMix);

    #ifdef ARPEGGIATOR
        // const float ph = phasorOsc.phasor(1);
        // const bool euclidNewNote = euclidean(ph, 12, euclidN, 0, 0.1f);
        // if(zxdetect.onZX(euclidNewNote)) {
        //     envamp=0.8f;
        //     freqIndex++;
        //     if(freqIndex >= nFREQs) {
        //         freqIndex = 0;
        //     }
        //     baseFreq = frequencies[freqIndex];
        // }else{
        //     // constexpr float envdec = 0.2f/9000.f;
        //     envamp -= envdec;
        //     if (envamp < 0.f) {
        //         envamp = 0.f;
        //     }
        // }
    #else
        // if(newNote) {
        //     newNote = false;
        //     envamp=0.8f;
        // }else{
        //     // constexpr float envdec = 0.2f/9000.f;
        //     envamp -= envdec;
        //     if (envamp < 0.f) {
        //         envamp = 0.f;
        //     }
        // }
    #endif
        float envval = env.play();
        y = y * envval;

    // #ifndef ARPEGGIATOR
    //     y *= noteVel;
    // #endif

        y = tanhf(y);
        
        float d1 = (dl1.play(y, delayMax, dlfb) * dl1mix);
        y = y + d1;// + d2;
        stereosample_t ret { y, y };
        feedback = y * feedbackGain;
        // frame++;
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

        arpFreq = frequencies[0];
        envamp=1.f;

        env.setup(500,500,0.8,1000,sampleRatef);

        queue_init(&qMIDINoteOn, sizeof(uint8_t)*2, 1);
        queue_init(&qMIDINoteOff, sizeof(uint8_t)*2, 1);

    }

    inline float mtof(uint8_t note) {
        // Convert MIDI note to frequency
        return 440.0f * exp2f((note - 69) / 12.0f);
    }

    size_t currNote=0;
    void loop() override {
        uint8_t midimsg[2];
        if (firstParamsReceived && queue_try_remove(&qMIDINoteOn, &midimsg)) {
            // Serial.printf("PAFSynthAudioApp::ProcessParams - Received MIDI Note On: %d, Velocity: %d\n", midimsg[0], midimsg[1]);
            baseFreq = mtof(midimsg[0]);
            // Serial.printf("PAFSynthAudioApp::loop - Note On: %d, Freq: %.2f Hz, Velocity: %d\n", midimsg[0], baseFreq, midimsg[1]); 
            noteVel = midimsg[1] / 127.0f; // Normalize velocity to [0, 1]
            noteVel = noteVel * noteVel; // Square the velocity for more pronounced effect
            newNote = true;
            env.trigger(noteVel);
            currNote = midimsg[0];
        }
        if (firstParamsReceived && queue_try_remove(&qMIDINoteOff, &midimsg)) {
            // Serial.printf("PAFSynthAudioApp::ProcessParams - Received MIDI Note On: %d, Velocity: %d\n", midimsg[0], midimsg[1]);
            if (currNote == midimsg[0]) {
                env.release();
            }
        }
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params)
    {
        firstParamsReceived = true;
        currentVoiceSpace(params);
    }

    queue_t qMIDINoteOn, qMIDINoteOff;

    

protected:

    maxiPAFOperator paf0;
    maxiPAFOperator paf1;
    maxiPAFOperator paf2;
    maxiPAFOperator paf3;

    maxiDelayline<10000> dl1;
    // maxiDelayline<15100> dl2;

    maxiOsc pulse;

    ADSRLite env;

    float frame=0;

    float feedback=0.f, feedbackGain=0.f;

    float p0Gain=1.f, p1Gain = 1.f, p2Gain=1.f, p3Gain=1.f;

    float paf0_freq = 100;
    float paf1_freq = 100;
    float paf2_freq = 50;
    float paf3_freq = 50;

    float paf0_cf = 200;
    float paf1_cf = 250;
    float paf2_cf = 250;
    float paf3_cf = 250;

    float paf0_bw = 100;
    float paf1_bw = 5000;
    float paf2_bw = 5000;
    float paf3_bw = 5000;

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

    float dl1mix = 0.0f;
    float dl2mix = 0.0f;
    float dlfb = 0.5f;

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

    maxiOsc phasorOsc;
    maxiTrigger zxdetect;

    size_t euclidN=4;

    float baseFreq = 50.0f; // Base frequency for the synth
    bool newNote=false;
    float noteVel = 0.f;
    bool firstParamsReceived = false;

    float envdec=0.2f/9000.f; // Decay rate for the envelope

    float sampleRatef = maxiSettings::getSampleRate();

    float fbzm1=0.f;
    size_t delayMax=10;
    float fbSmoothAlpha=0.95f;


};

#endif  // __PAF_SYNTH_AUDIO_APP_HPP__
