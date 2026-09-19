#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

const unsigned long SAMPLE_INTERVAL_MS = 20; // 50 Hz = 1/0.020 seconds = 20ms
unsigned long lastSampleTime = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for serial console to open
  }

  // Initialize I2C with Fast Mode (400 kHz)
  Wire.begin();
  Wire.setClock(400000);

  if (!mpu.begin()) {
    Serial.println("Failed to find MPU6050 chip");
    while (1) {
      delay(10);
    }
  }

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