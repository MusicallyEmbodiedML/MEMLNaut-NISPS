// pico2w_bno055_test — pico-sdk I2C (blocking, handles BNO055 clock-stretching)
//
// Wiring:
//   BNO055 SDA  -> GP4      BNO055 VIN  -> 5V (VBUS)
//   BNO055 SCL  -> GP5      BNO055 GND  -> GND
//   BNO055 ADR  -> GND      BNO055 BOOT -> GND
//   RST, INT    -> leave unconnected

#include <Arduino.h>
#include "hardware/i2c.h"

#define BNO_I2C    i2c0
#define SDA_PIN    4
#define SCL_PIN    5
#define BNO_ADDR   0x28

#define REG_CHIP_ID    0x00
#define REG_OPR_MODE   0x3D
#define REG_CALIB_STAT 0x35
#define REG_EUL_H_LSB  0x1A
#define MODE_CONFIG    0x00
#define MODE_NDOF      0x0C
#define BNO_ID         0xA0

void bno_write(uint8_t reg, uint8_t val) {
    uint8_t buf[2] = {reg, val};
    i2c_write_blocking(BNO_I2C, BNO_ADDR, buf, 2, false);
}

uint8_t bno_read(uint8_t reg) {
    uint8_t val = 0xFF;
    i2c_write_blocking(BNO_I2C, BNO_ADDR, &reg, 1, true);  // nostop=true (repeated start)
    sleep_us(500);
    i2c_read_blocking(BNO_I2C, BNO_ADDR, &val, 1, false);
    return val;
}

int16_t bno_read16(uint8_t reg) {
    uint8_t buf[2] = {0};
    i2c_write_blocking(BNO_I2C, BNO_ADDR, &reg, 1, true);  // nostop=true
    sleep_us(500);
    i2c_read_blocking(BNO_I2C, BNO_ADDR, buf, 2, false);
    return (int16_t)((buf[1] << 8) | buf[0]);
}

void setup() {
    Serial.begin(115200);
    while (!Serial) delay(10);
    Serial.println("BNO055 direct init");

    // I2C bus recovery: clock SCL 9 times to release any stuck slave
    gpio_init(SDA_PIN); gpio_set_dir(SDA_PIN, GPIO_OUT); gpio_put(SDA_PIN, 1);
    gpio_init(SCL_PIN); gpio_set_dir(SCL_PIN, GPIO_OUT);
    for (int i = 0; i < 9; i++) {
        gpio_put(SCL_PIN, 0); delay(1);
        gpio_put(SCL_PIN, 1); delay(1);
    }
    // STOP condition
    gpio_put(SDA_PIN, 0); delay(1);
    gpio_put(SCL_PIN, 1); delay(1);
    gpio_put(SDA_PIN, 1); delay(1);

    i2c_init(BNO_I2C, 10000);  // 10kHz — slow for RP2040 errata E18 repeated start
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    delay(500);

    // Ensure page 0
    i2c_write_blocking(BNO_I2C, BNO_ADDR, (uint8_t[]){0x07, 0x00}, 2, false);
    delay(10);

    uint8_t id = bno_read(REG_CHIP_ID);

    if (id != BNO_ID) {
        Serial.println("ERROR: expected 0xA0");
        while (true) delay(1000);
    }

    bno_write(REG_OPR_MODE, MODE_CONFIG);  delay(25);
    bno_write(REG_OPR_MODE, MODE_NDOF);   delay(20);
    Serial.println("OK - heading, roll, pitch, calib");
}

void loop() {
    float heading = bno_read16(REG_EUL_H_LSB)     / 16.0f;
    float roll    = bno_read16(REG_EUL_H_LSB + 2) / 16.0f;
    float pitch   = bno_read16(REG_EUL_H_LSB + 4) / 16.0f;
    uint8_t calib = bno_read(REG_CALIB_STAT);

    Serial.print(heading, 1); Serial.print(", ");
    Serial.print(roll, 1);    Serial.print(", ");
    Serial.print(pitch, 1);   Serial.print(",  calib=0x");
    Serial.println(calib, HEX);
    delay(20);
}
