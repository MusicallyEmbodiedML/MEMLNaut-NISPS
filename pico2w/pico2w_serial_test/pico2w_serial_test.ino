// pico2w_serial_test
//
// Sends a SLIP-encoded vector of floats to the MEMLNaut over UART at 50 Hz.
// Uses Arduino-Pico SDK (Earle Philhower).
//
// Wiring:
//   Pico2W GP0 (Serial1 TX) -> MEMLNaut pin 35 (SENSOR_RX)
//   Pico2W GP1 (Serial1 RX) -> MEMLNaut pin 34 (SENSOR_TX)  [optional]
//   Pico2W GND              -> MEMLNaut GND
//
// N_FLOATS must match kN_SerialInputs in MEMLNautModeBunty.hpp (default: 3)

#include <Arduino.h>
#include <math.h>

constexpr size_t   N_FLOATS          = 3;
constexpr uint32_t SEND_INTERVAL_MS  = 20;   // 50 Hz
constexpr uint32_t BAUD_RATE         = 115200;

// SLIP special bytes (RFC 1055)
static constexpr uint8_t SLIP_END     = 0xC0;
static constexpr uint8_t SLIP_ESC     = 0xDB;
static constexpr uint8_t SLIP_ESC_END = 0xDC;
static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

// Send a single byte with SLIP escaping
static void slip_write_byte(uint8_t b) {
    if (b == SLIP_END) {
        Serial1.write(SLIP_ESC);
        Serial1.write(SLIP_ESC_END);
    } else if (b == SLIP_ESC) {
        Serial1.write(SLIP_ESC);
        Serial1.write(SLIP_ESC_ESC);
    } else {
        Serial1.write(b);
    }
}

// Encode and send an array of floats as a SLIP packet
static void slip_send(const float* data, size_t n) {
    Serial1.write(SLIP_END);                    // packet start
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n * sizeof(float); i++) {
        slip_write_byte(raw[i]);
    }
    Serial1.write(SLIP_END);                    // packet end
}

void setup() {
    Serial1.setTX(0);   // GP0
    Serial1.setRX(1);   // GP1
    Serial1.begin(BAUD_RATE);
    Serial.begin();
}

void loop() {
    float values[N_FLOATS];

    // Test pattern: independent slow sine sweeps on each channel, [0, 1] range
    float t = millis() / 1000.0f;
    for (size_t i = 0; i < N_FLOATS; i++) {
        float phase_offset = i * (2.0f * M_PI / 3.0f);   // 120° apart
        values[i] = 0.5f + 0.5f * sinf(0.5f * t + phase_offset);
    }

    slip_send(values, N_FLOATS);
    delay(SEND_INTERVAL_MS);
    Serial.println(".");
}
