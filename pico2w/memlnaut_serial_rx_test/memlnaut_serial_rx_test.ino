// memlnaut_serial_rx_test
//
// Receives SLIP-encoded float packets from the Pico2W serial test sketch
// and prints the decoded values over USB Serial.
//
// Wiring (matches pico2w_serial_test):
//   MEMLNaut pin 35 (SENSOR_RX) <- Pico2W GP0 (Serial1 TX)
//   MEMLNaut pin 34 (SENSOR_TX) -> Pico2W GP1 (Serial1 RX)  [optional]
//   MEMLNaut GND                -- Pico2W GND
//
// N_FLOATS must match N_FLOATS in pico2w_serial_test (default: 3)

#include <Arduino.h>

constexpr size_t   N_FLOATS   = 3;
constexpr uint32_t BAUD_RATE  = 115200;

// SENSOR_RX = pin 35, SENSOR_TX = pin 34  (from Pins.hpp)
constexpr uint8_t SENSOR_RX = 35;
constexpr uint8_t SENSOR_TX = 34;

// SLIP special bytes (RFC 1055)
static constexpr uint8_t SLIP_END     = 0xC0;
static constexpr uint8_t SLIP_ESC     = 0xDB;
static constexpr uint8_t SLIP_ESC_END = 0xDC;
static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

// Decoder state
static uint8_t  slip_buf[N_FLOATS * sizeof(float)];
static size_t   slip_len   = 0;
static bool     in_packet  = false;
static bool     escape_next = false;

// Packet and error counters
static uint32_t packets_rx  = 0;
static uint32_t slip_errors = 0;
static uint32_t last_report_ms = 0;

static void process_packet() {
    size_t expected = N_FLOATS * sizeof(float);
    if (slip_len != expected) {
        slip_errors++;
        Serial.print("SLIP: bad packet length ");
        Serial.print(slip_len);
        Serial.print(" (expected ");
        Serial.print(expected);
        Serial.println(")");
        return;
    }

    float values[N_FLOATS];
    memcpy(values, slip_buf, expected);
    packets_rx++;

    Serial.print("pkt ");
    Serial.print(packets_rx);
    Serial.print(":  ");
    for (size_t i = 0; i < N_FLOATS; i++) {
        Serial.print(values[i], 4);
        if (i + 1 < N_FLOATS) Serial.print("  ");
    }
    Serial.println();
}

static void slip_feed(uint8_t b) {
    if (escape_next) {
        escape_next = false;
        if      (b == SLIP_ESC_END) b = SLIP_END;
        else if (b == SLIP_ESC_ESC) b = SLIP_ESC;
        else {
            slip_errors++;
            in_packet = false;
            return;
        }
        if (slip_len < sizeof(slip_buf)) slip_buf[slip_len++] = b;
        return;
    }

    if (b == SLIP_END) {
        if (in_packet && slip_len > 0) {
            process_packet();
        }
        // Start (or restart) a new packet
        in_packet  = true;
        slip_len   = 0;
        return;
    }

    if (!in_packet) return;

    if (b == SLIP_ESC) {
        escape_next = true;
        return;
    }

    if (slip_len < sizeof(slip_buf)) {
        slip_buf[slip_len++] = b;
    } else {
        // Buffer overflow — discard packet
        slip_errors++;
        in_packet = false;
    }
}

void setup() {
    Serial.begin(115200);
    delay(1500);   // give USB serial time to connect

    Serial1.setRX(SENSOR_RX);
    Serial1.setTX(SENSOR_TX);
    Serial1.begin(BAUD_RATE);

    Serial.println("memlnaut_serial_rx_test");
    Serial.print("Listening on SENSOR_RX (pin ");
    Serial.print(SENSOR_RX);
    Serial.print(") at ");
    Serial.print(BAUD_RATE);
    Serial.println(" baud");
    Serial.println("Expecting " + String(N_FLOATS) + " floats per packet (SLIP)");
    Serial.println("---");
}

void loop() {
    while (Serial1.available()) {
        slip_feed(static_cast<uint8_t>(Serial1.read()));
    }

    // Print a heartbeat / stats line every 5 s even if nothing arrives
    uint32_t now = millis();
    if (now - last_report_ms >= 5000) {
        last_report_ms = now;
        if (packets_rx == 0 && slip_errors == 0) {
            Serial.println("[waiting for data...]");
        } else {
            Serial.print("[stats] packets=");
            Serial.print(packets_rx);
            Serial.print("  errors=");
            Serial.println(slip_errors);
        }
    }
}
