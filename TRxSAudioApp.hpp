#ifndef __TRXS_AUDIO_APP_HPP__
#define __TRXS_AUDIO_APP_HPP__

#include "src/memllib/audio/AudioAppBase.hpp"
#include "src/memllib/interface/InterfaceBase.hpp"
#include "voicespaces/VoiceSpaces.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <array>

template<size_t NPARAMS=16>
class TRxSAudioApp : public AudioAppBase<NPARAMS>
{
public:
    static constexpr size_t kN_Params = NPARAMS;
    static constexpr size_t nVoiceSpaces = 1;

    std::array<VoiceSpace<NPARAMS>, nVoiceSpaces> voiceSpaces;
    VoiceSpaceFn<NPARAMS> currentVoiceSpace;

    std::array<String, nVoiceSpaces> getVoiceSpaceNames() {
        std::array<String, nVoiceSpaces> names;
        for (size_t i = 0; i < voiceSpaces.size(); i++) {
            names[i] = voiceSpaces[i].name;
        }
        return names;
    }

    void setVoiceSpace(size_t i) {
        if (i < voiceSpaces.size()) {
            currentVoiceSpace = voiceSpaces[i].mappingFunction;
        }
    }

    TRxSAudioApp() : AudioAppBase<NPARAMS>() {
        voiceSpaces[0] = {"Default", [this](const std::array<float, NPARAMS>& params) {
            for (size_t i = 0; i < NPARAMS; i++) {
                trxsParams[i] = params[i];
            }
        }};
        currentVoiceSpace = voiceSpaces[0].mappingFunction;
    }

    stereosample_t __force_inline Process(const stereosample_t x) override {
        return {0.f, 0.f};
    }

    void Setup(float sample_rate, std::shared_ptr<InterfaceBase> interface) override {
        AudioAppBase<NPARAMS>::Setup(sample_rate, interface);
        queue_init(&qMIDINoteOn,  sizeof(uint8_t) * 2, 1);
        queue_init(&qMIDINoteOff, sizeof(uint8_t) * 2, 1);
    }

    void loop() override {
        uint8_t midimsg[2];
        queue_try_remove(&qMIDINoteOn,  &midimsg);
        queue_try_remove(&qMIDINoteOff, &midimsg);
        AudioAppBase<NPARAMS>::loop();
    }

    void ProcessParams(const std::array<float, NPARAMS>& params) {
        firstParamsReceived = true;
        if (currentVoiceSpace) currentVoiceSpace(params);
    }

    std::array<float, NPARAMS> trxsParams{};

    queue_t qMIDINoteOn, qMIDINoteOff;

protected:
    bool firstParamsReceived = false;
};

#endif  // __TRXS_AUDIO_APP_HPP__
