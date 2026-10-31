#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

// I2C pins are set explicitly rather than relying on the board variant's
// defaults. These match the ESP32-C3 SuperMini's nominal SDA=8 / SCL=9.
//
// Caveat worth knowing if the bus misbehaves: GPIO8 also drives the onboard
// LED and GPIO9 is the BOOT strapping pin, so both carry extra loads, and a
// device holding SCL low at reset can drop the chip into download mode.
// GPIO6/7 are free of both issues. Override from platformio.ini to move them.
#ifndef MPU_SDA_PIN
#define MPU_SDA_PIN 8
#endif
#ifndef MPU_SCL_PIN
#define MPU_SCL_PIN 9
#endif

const unsigned long SAMPLE_INTERVAL_MS = 20; // 50 Hz = 1/0.020 seconds = 20ms
unsigned long lastSampleTime = 0;

// MPU6050 responds at 0x68, or 0x69 when the AD0 pin is pulled high.
static const uint8_t MPU_ADDR_LOW = 0x68;
static const uint8_t MPU_ADDR_HIGH = 0x69;
static const uint8_t MPU_REG_WHO_AM_I = 0x75;

// Report every device answering on the bus, so a wiring or address problem is
// visible in the monitor instead of showing up as silence.
static uint8_t scanI2CBus() {
  Serial.print("I2C scan:");
  uint8_t found = 0;
  for (uint8_t address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    if (Wire.endTransmission() == 0) {
      Serial.print(" 0x");
      Serial.print(address, HEX);
      found++;
    }
  }
  if (found == 0) {
    Serial.print(" no devices found");
  }
  Serial.println();
  return found;
}

// Read WHO_AM_I directly. Adafruit_MPU6050::begin() only accepts 0x68 here, so
// a module that answers on the bus but reports something else is a MPU6500 /
// MPU9250 clone rather than a genuine MPU6050.
static bool readWhoAmI(uint8_t address, uint8_t *value) {
  Wire.beginTransmission(address);
  Wire.write(MPU_REG_WHO_AM_I);
  if (Wire.endTransmission() != 0) {
    return false;
  }
  if (Wire.requestFrom((int)address, 1) != 1) {
    return false;
  }
  *value = Wire.read();
  return true;
}

// Candidate GPIOs for a bus sweep on the ESP32-C3 SuperMini. Deliberately
// excluded:
//   GPIO11-17 - internal SPI flash; driving these crashes the running firmware
//   GPIO18/19 - native USB D-/D+; driving these kills this very serial link
static const uint8_t CANDIDATE_PINS[] = {0, 1, 2,  3,  4, 5, 6,
                                         7, 8, 9, 10, 20, 21};
static const size_t CANDIDATE_PIN_COUNT =
    sizeof(CANDIDATE_PINS) / sizeof(CANDIDATE_PINS[0]);

static bool probeAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

// Brute-force every ordered SDA/SCL combination and report any pair that gets
// an ACK at an MPU6050 address. Ordered, not combinations, so a swapped pair
// is found too.
static void sweepI2CPins() {
  Serial.println();
  Serial.println("Sweeping all usable GPIO pairs for an MPU6050 (0x68/0x69)...");
  Serial.println("(skipping GPIO11-17 = SPI flash, GPIO18/19 = USB)");

  uint8_t hits = 0;
  for (size_t i = 0; i < CANDIDATE_PIN_COUNT; i++) {
    Serial.print(".");
    for (size_t j = 0; j < CANDIDATE_PIN_COUNT; j++) {
      if (i == j) {
        continue;
      }
      uint8_t sda = CANDIDATE_PINS[i];
      uint8_t scl = CANDIDATE_PINS[j];

      Wire.end();
      if (!Wire.begin(sda, scl, 100000)) {
        continue;
      }
      Wire.setTimeOut(20); // keep a full sweep to a few seconds
      delay(5);

      for (uint8_t k = 0; k < 2; k++) {
        uint8_t address = (k == 0) ? MPU_ADDR_LOW : MPU_ADDR_HIGH;
        if (!probeAddress(address)) {
          continue;
        }
        hits++;
        Serial.println();
        Serial.print("  HIT  SDA=");
        Serial.print(sda);
        Serial.print(" SCL=");
        Serial.print(scl);
        Serial.print(" addr=0x");
        Serial.print(address, HEX);
        uint8_t id = 0;
        if (readWhoAmI(address, &id)) {
          Serial.print(" WHO_AM_I=0x");
          Serial.print(id, HEX);
        }
        Serial.println();
      }
    }
  }

  Serial.println();
  if (hits == 0) {
    Serial.println("  Nothing answered on any pin pair.");
    Serial.println("  That points at power or the module itself, not the pins:");
    Serial.println("  check 3V3 and GND at the module, then try another sensor.");
  }
  Serial.println("Sweep complete.");
  Serial.println();

  Wire.end();
}

static void reportWhoAmI(uint8_t address) {
  uint8_t id = 0;
  if (!readWhoAmI(address, &id)) {
    return;
  }
  Serial.print("  0x");
  Serial.print(address, HEX);
  Serial.print(" WHO_AM_I=0x");
  Serial.print(id, HEX);
  if (id == 0x68) {
    Serial.println(" (MPU6050)");
  } else if (id == 0x70) {
    Serial.println(" (MPU6500 clone - not supported by Adafruit_MPU6050)");
  } else if (id == 0x71 || id == 0x73) {
    Serial.println(" (MPU9250 clone - not supported by Adafruit_MPU6050)");
  } else {
    Serial.println(" (unknown chip)");
  }
}

void setup() {
  Serial.begin(115200);

  // Native USB CDC: give the host a chance to open the port, but time out so
  // the sketch still runs when powered from a battery or plain charger.
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 3000) {
    delay(10);
  }
  delay(500); // let the CDC endpoint settle so the first lines aren't dropped

  Serial.println();
  Serial.print("Booted. I2C SDA=");
  Serial.print(MPU_SDA_PIN);
  Serial.print(" SCL=");
  Serial.println(MPU_SCL_PIN);

  // Probe at Standard Mode (100 kHz). Long jumper wires plus the ESP32's weak
  // internal pull-ups are unreliable at 400 kHz; we raise the clock only once
  // the sensor has been identified.
  Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
  Wire.setClock(100000);

  // Retry instead of parking silently: the monitor is often attached after
  // boot, and a dead sketch looks identical to a broken serial link. The scan
  // repeats so the diagnostic is visible whenever you attach.
  while (!mpu.begin(MPU_ADDR_LOW) && !mpu.begin(MPU_ADDR_HIGH)) {
    Serial.println("Failed to find MPU6050 chip - check wiring, retrying...");
    if (scanI2CBus() > 0) {
      reportWhoAmI(MPU_ADDR_LOW);
      reportWhoAmI(MPU_ADDR_HIGH);
    }
    delay(1000);
  }
  Serial.println("MPU6050 initialized");

  // Sensor confirmed, switch to Fast Mode (400 kHz)
  Wire.setClock(400000);

  // Configure MPU6050 ranges suitable for fall impacts
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setGyroRange(MPU6050_RANGE_2000_DEG);
  mpu.setFilterBandwidth(MPU6050_BAND_44_HZ);

  // Print CSV header
  Serial.println("Timestamp_ms,AccX,AccY,AccZ,GyroX,GyroY,GyroZ");
}

void loop() {
  unsigned long currentTime = millis();

  // Non-blocking 50 Hz sampling loop
  if (currentTime - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = currentTime;

    sensors_event_t a, g, temp;
    mpu.getEvent(&a, &g, &temp);

    // Print reading in CSV format
    Serial.print(currentTime);
    Serial.print(",");
    Serial.print(a.acceleration.x);
    Serial.print(",");
    Serial.print(a.acceleration.y);
    Serial.print(",");
    Serial.print(a.acceleration.z);
    Serial.print(",");
    Serial.print(g.gyro.x);
    Serial.print(",");
    Serial.print(g.gyro.y);
    Serial.print(",");
    Serial.println(g.gyro.z);
  }
}