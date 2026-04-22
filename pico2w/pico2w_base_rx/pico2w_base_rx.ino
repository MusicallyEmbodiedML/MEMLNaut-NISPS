// pico2w_base_rx
//
// MEMLNaut wireless sensor base station.
// Creates a WiFi soft-AP, receives UDP packets from up to N_BOARDS sensor
// Pico2W boards, and forwards merged sensor data over SLIP serial at 50 Hz.
//
// Output packet layout: [ board0[0..2], board1[0..2] ] — 6 floats total.
// Set N_FLOATS = N_BOARDS * N_FLOATS_PER_BOARD (= 6) on the MEMLNaut side.
//
// Wiring:
//   Pico2W GP4 (Serial2 TX) -> MEMLNaut pin 35 (SENSOR_RX)
//   Pico2W GP5 (Serial2 RX) -> MEMLNaut pin 34 (SENSOR_TX)  [optional]
//   Pico2W GND              -> MEMLNaut GND

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>

// ── Configuration ─────────────────────────────────────────────────────────────
constexpr size_t   N_BOARDS           = 2;
constexpr size_t   N_FLOATS_PER_BOARD = 3;
constexpr uint32_t SEND_INTERVAL_MS   = 20;      // 50 Hz serial output
constexpr uint32_t BAUD_RATE          = 115200;

// Uncomment to enable debug output on USB Serial
#define DEBUG

// How often to print the periodic stats summary (ms); only used when DEBUG is on
constexpr uint32_t DEBUG_STATS_INTERVAL_MS = 2000;
// ─────────────────────────────────────────────────────────────────────────────

// Shared constants — keep in sync with pico2w_sensor_tx
static constexpr char     AP_SSID[] = "MEMLNAUT-NET";
static constexpr char     AP_PASS[] = "memlnaut1";
static constexpr uint16_t UDP_PORT  = 4210;

static constexpr size_t TOTAL_FLOATS = N_BOARDS * N_FLOATS_PER_BOARD;
static constexpr size_t UDP_PKT_SIZE = 1 + N_FLOATS_PER_BOARD * sizeof(float);

// SLIP special bytes (RFC 1055)
static constexpr uint8_t SLIP_END     = 0xC0;
static constexpr uint8_t SLIP_ESC     = 0xDB;
static constexpr uint8_t SLIP_ESC_END = 0xDC;
static constexpr uint8_t SLIP_ESC_ESC = 0xDD;

static WiFiUDP udp;
static float   board_values[N_BOARDS][N_FLOATS_PER_BOARD] = {};

#ifdef DEBUG
static uint32_t dbg_pkts_rx[N_BOARDS]  = {};
static uint32_t dbg_pkts_bad           = 0;
static uint32_t dbg_slips_sent         = 0;
static uint32_t dbg_last_rx_ms[N_BOARDS] = {};
#endif

static void slip_write_byte(uint8_t b) {
    if (b == SLIP_END) {
        Serial2.write(SLIP_ESC);
        Serial2.write(SLIP_ESC_END);
    } else if (b == SLIP_ESC) {
        Serial2.write(SLIP_ESC);
        Serial2.write(SLIP_ESC_ESC);
    } else {
        Serial2.write(b);
    }
}

static void slip_send(const float* data, size_t n) {
    Serial2.write(SLIP_END);
    const uint8_t* raw = reinterpret_cast<const uint8_t*>(data);
    for (size_t i = 0; i < n * sizeof(float); i++) {
        slip_write_byte(raw[i]);
    }
    Serial2.write(SLIP_END);
}

void setup() {
    
    Serial.begin();
    Serial.println("Hi");

    WiFi.softAP(AP_SSID, AP_PASS);
    delay(500);   // allow AP interface to fully initialise before binding
    udp.begin(UDP_PORT);

    // Serial2 (UART1, GP4/GP5) used instead of Serial1 — CYW43 driver reclaims GP0/GP1
    Serial2.setTX(4);   // GP4
    Serial2.setRX(5);   // GP5
    Serial2.begin(BAUD_RATE);
    Serial.print("Serial2 ready: "); Serial.println(Serial2 ? "yes" : "NO");

    // Raw UART smoke-test — send a recognisable pattern immediately after init
    // MEMLNaut should see raw_bytes increment by 4 on the next stats line
    Serial2.write((uint8_t)0xAA);
    Serial2.write((uint8_t)0x55);
    Serial2.write((uint8_t)0xAA);
    Serial2.write((uint8_t)0x55);

    Serial.print("AP started: ");
    Serial.print(AP_SSID);
    Serial.print("  IP: ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Listening on UDP port ");
    Serial.println(UDP_PORT);
}

void loop() {
    
    // Drain all incoming UDP packets, updating per-board value buffers
    int pkt_size;
    while ((pkt_size = udp.parsePacket()) > 0) {
        if (pkt_size == (int)UDP_PKT_SIZE) {
            uint8_t buf[UDP_PKT_SIZE];
            udp.read(buf, UDP_PKT_SIZE);
            uint8_t id = buf[0];
            if (id < N_BOARDS) {
                memcpy(board_values[id], buf + 1, N_FLOATS_PER_BOARD * sizeof(float));
#ifdef DEBUG
                dbg_pkts_rx[id]++;
                dbg_last_rx_ms[id] = millis();
                // Serial.print("[rx] board=");
                // Serial.print(id);
                // Serial.print("  ");
                // for (size_t i = 0; i < N_FLOATS_PER_BOARD; i++) {
                //     Serial.print(board_values[id][i], 3);
                //     if (i + 1 < N_FLOATS_PER_BOARD) Serial.print("  ");
                // }
                // Serial.println();
#endif
            }
#ifdef DEBUG
            else {
                Serial.print("[rx] unknown board_id=");
                Serial.println(id);
                dbg_pkts_bad++;
            }
#endif
        } else {
            // Discard malformed packet
            uint8_t discard[64];
            while (udp.available()) udp.read(discard, sizeof(discard));
#ifdef DEBUG
            dbg_pkts_bad++;
            Serial.print("[rx] bad packet size=");
            Serial.println(pkt_size);
#endif
        }
    }

    // Send merged SLIP packet at SEND_INTERVAL_MS rate
    static uint32_t last_send_ms = 0;
    uint32_t now = millis();
    if (now - last_send_ms >= SEND_INTERVAL_MS) {
        last_send_ms = now;

        float merged[TOTAL_FLOATS];
        for (size_t b = 0; b < N_BOARDS; b++) {
            memcpy(merged + b * N_FLOATS_PER_BOARD,
                   board_values[b],
                   N_FLOATS_PER_BOARD * sizeof(float));
        }
        slip_send(merged, TOTAL_FLOATS);
#ifdef DEBUG
        dbg_slips_sent++;
#endif
    }

#ifdef DEBUG
    // Periodic stats: packet counts, per-board silence detection
    static uint32_t dbg_last_stats_ms = 0;
    if (now - dbg_last_stats_ms >= DEBUG_STATS_INTERVAL_MS) {
        dbg_last_stats_ms = now;
        Serial.print("[stats] slips=");
        Serial.print(dbg_slips_sent);
        Serial.print("  bad=");
        Serial.print(dbg_pkts_bad);
        Serial.print("  clients=");
        Serial.print(WiFi.softAPgetStationNum());
        for (size_t b = 0; b < N_BOARDS; b++) {
            Serial.print("  board");
            Serial.print(b);
            Serial.print("=");
            uint32_t silence = (dbg_last_rx_ms[b] == 0) ? now : now - dbg_last_rx_ms[b];
            if (dbg_pkts_rx[b] == 0) {
                Serial.print("(no data)");
            } else {
                Serial.print(dbg_pkts_rx[b]);
                Serial.print("pkts/");
                Serial.print(silence);
                Serial.print("ms ago");
            }
        }
        Serial.println();
    }
#endif
}
