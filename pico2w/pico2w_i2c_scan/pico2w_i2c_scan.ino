// I2C scanner — scans one pin pair, prints found addresses.
// Change SDA_PIN / SCL_PIN to match your wiring and re-upload.

#include <Wire.h>

#define SDA_PIN 4
#define SCL_PIN 5

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);

    Wire.setSDA(SDA_PIN);
    Wire.setSCL(SCL_PIN);
    Wire.setClock(100000);  // 100 kHz — conservative
    Wire.begin();

    Serial.print("Scanning GP"); Serial.print(SDA_PIN);
    Serial.print(" SDA / GP"); Serial.print(SCL_PIN); Serial.println(" SCL at 100kHz");

    int found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0) {
            Serial.print("  Found: 0x");
            if (addr < 16) Serial.print("0");
            Serial.println(addr, HEX);
            found++;
        }
        delay(1);
    }
    Serial.print(found); Serial.println(found == 1 ? " device found." : " devices found.");
    Serial.println("----");
    Serial.println("If nothing found, change SDA_PIN/SCL_PIN and re-upload.");
    Serial.println("Try: 0/1, 2/3, 4/5, 6/7, 8/9, 10/11, 12/13, 14/15, 16/17, 20/21, 26/27");
}

void loop() {}
