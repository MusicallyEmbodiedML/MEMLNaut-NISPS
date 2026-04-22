// pico2w_sensor_tx
//
// Wireless sensor transmitter for MEMLNaut.
// Connects to the base Pico2W soft-AP and sends sensor data at 50 Hz via UDP.
// Optimised for 18650 battery operation.
//
// To program the second board, change BOARD_ID to 1.
// Replace read_sensors() with your actual sensor reads.
//
// No wiring required beyond power.

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <math.h>

// ── User configuration ────────────────────────────────────────────────────────
constexpr uint8_t  BOARD_ID         = 0;     // 0 for first board, 1 for second
constexpr size_t   N_FLOATS         = 3;     // must match N_FLOATS_PER_BOARD in base
constexpr uint32_t SEND_INTERVAL_MS = 20;   // 50 Hz; set to 10 for 100 Hz
#define DEBUG_TX                             // comment out to disable serial debug
// ─────────────────────────────────────────────────────────────────────────────

// Shared constants — keep in sync with pico2w_base_rx
static constexpr char     AP_SSID[] = "MEMLNAUT-NET";
static constexpr char     AP_PASS[] = "memlnaut1";
static constexpr uint16_t UDP_PORT  = 4210;
// Broadcast rather than unicast — avoids lwIP routing quirks on the AP interface
static const     IPAddress BASE_IP(255, 255, 255, 255);

// Packet layout: [ board_id (1 byte) | floats (N_FLOATS × 4 bytes) ]
static constexpr size_t PACKET_SIZE = 1 + N_FLOATS * sizeof(float);

static WiFiUDP udp;

// ── Sensor stub ───────────────────────────────────────────────────────────────
// Replace this with your real sensor reads.
// out[] must be filled with N_FLOATS values, normalised to [0, 1] recommended.
static void read_sensors(float out[N_FLOATS]) {
    float t = millis() / 1000.0f;
    for (size_t i = 0; i < N_FLOATS; i++) {
        float phase = i * (2.0f * M_PI / 3.0f);
        out[i] = 0.5f + 0.5f * sinf(t + phase);
    }
}
// ─────────────────────────────────────────────────────────────────────────────

static void wifi_connect() {
    // WiFi.disconnect();
#ifdef DEBUG_TX
    Serial.print("Connecting to ");
    Serial.println(AP_SSID);
#endif
    WiFi.begin(AP_SSID, AP_PASS);
    while (WiFi.status() != WL_CONNECTED) {
        delay(100);
    }
    udp.stop();
    udp.begin(UDP_PORT);
#ifdef DEBUG_TX
    Serial.print("Connected, IP: ");
    Serial.println(WiFi.localIP());
#endif
}

void setup() {
#ifdef DEBUG_TX
    Serial.begin(115200);
    delay(1000);
    Serial.println("pico2w_sensor_tx starting");
#endif

    // LED off — it draws a few mA through the CYW43 GPIO
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    wifi_connect();  // also calls udp.begin()
}

void loop() {
    uint32_t t0 = millis();

    if (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG_TX
        Serial.println("WiFi lost — reconnecting");
#endif
        wifi_connect();
    }

    float values[N_FLOATS];
    read_sensors(values);

    uint8_t pkt[PACKET_SIZE];
    pkt[0] = BOARD_ID;
    memcpy(pkt + 1, values, N_FLOATS * sizeof(float));

    udp.beginPacket(BASE_IP, UDP_PORT);
    udp.write(pkt, PACKET_SIZE);
    int ok = udp.endPacket();

#ifdef DEBUG_TX
    static uint32_t pkt_count = 0;
    ++pkt_count;
    if (pkt_count % 50 == 0) {   // print every 50 packets (~1 s at 50 Hz)
        Serial.print("TX #");
        Serial.print(pkt_count);
        Serial.print("  board=");
        Serial.print(BOARD_ID);
        Serial.print("  [");
        for (size_t i = 0; i < N_FLOATS; i++) {
            Serial.print(values[i], 3);
            if (i < N_FLOATS - 1) Serial.print(", ");
        }
        Serial.print("]  ");
        Serial.println(ok ? "ok" : "FAIL");
    }
#endif

    uint32_t elapsed = millis() - t0;
    if (elapsed < SEND_INTERVAL_MS) {
        delay(SEND_INTERVAL_MS - elapsed);
    }
}
