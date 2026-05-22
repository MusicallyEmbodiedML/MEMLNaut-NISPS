#pragma once

#include "../src/memllib/interface/SynthParamOutput.hpp"
#include <vector>
#include <cstdint>
#include <algorithm>

// Roland D50 SysEx output using the DT1 (Data Set 1) format.
//
// DT1 frame: F0 41 <deviceId> 14 12 <addrHi> <addrMid> <addrLo> <value> <checksum> F7
// Checksum = (128 - ((addrHi + addrMid + addrLo + value) % 128)) % 128
//
// Call setParamMappings() to define which D50 parameters each NN output controls.
// This can be called again when switching voice spaces to retarget different parameters.
class D50SysExOutput : public SynthParamOutput {
public:
    struct ParamMapping {
        uint8_t addrHi;
        uint8_t addrMid;
        uint8_t addrLo;
        uint8_t minVal;
        uint8_t maxVal;
    };

    D50SysExOutput(std::shared_ptr<MIDIInOut> midi, uint8_t deviceId = 0x10)
        : SynthParamOutput(midi), deviceId_(deviceId) {}

    void setParamMappings(std::vector<ParamMapping> mappings) {
        mappings_ = std::move(mappings);
    }

    void sendParams(std::span<const float> params) override {
        // D50 spec requires ≥20ms between DT1 messages; enforce 30ms between full updates
        unsigned long now = millis();
        if (now - lastSendMs_ < 30) return;
        lastSendMs_ = now;

        size_t n = std::min(params.size(), mappings_.size());
        for (size_t i = 0; i < n; i++) {
            const auto& m = mappings_[i];
            float clamped = std::max(0.f, std::min(1.f, params[i]));
            uint8_t value = static_cast<uint8_t>(m.minVal + clamped * (m.maxVal - m.minVal));
            sendDT1(m.addrHi, m.addrMid, m.addrLo, value);
        }
    }

private:
    uint8_t deviceId_;
    std::vector<ParamMapping> mappings_;
    unsigned long lastSendMs_ = 0;

    uint8_t checksum(uint8_t h, uint8_t m, uint8_t l, uint8_t v) {
        return (128 - ((h + m + l + v) % 128)) % 128;
    }

    void sendDT1(uint8_t addrHi, uint8_t addrMid, uint8_t addrLo, uint8_t value) {
        uint8_t msg[11] = {
            0xF0,
            0x41,        // Roland manufacturer ID
            deviceId_,
            0x14,        // D50 model ID
            0x12,        // DT1 command
            addrHi, addrMid, addrLo,
            value,
            checksum(addrHi, addrMid, addrLo, value),
            0xF7
        };
        midi_->sendRawBytes(msg, sizeof(msg));
    }
};
