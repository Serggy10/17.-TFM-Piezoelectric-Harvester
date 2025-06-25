#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SHTC3.h>
#include <Adafruit_VEML7700.h>
#include <INA226.h>  // Rob Tillaart's library

#define SDA_PIN 4
#define SCL_PIN 5

// Sensores
Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
Adafruit_VEML7700 veml = Adafruit_VEML7700();
INA226 ina(0x40, &Wire);  // Dirección I2C por defecto (0x40)

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Iniciando sensores...");

  Wire.begin(SDA_PIN, SCL_PIN);
  delay(1000);

  // --- SHTC3 ---
  if (!shtc3.begin(&Wire)) {
    Serial.println("ERROR: SHTC3 no detectado. Intentando reset...");
    shtc3.reset();
    delay(100);
    if (!shtc3.begin(&Wire)) {
      Serial.println("ERROR: SHTC3 no disponible tras reset.");
    }
  } else {
    Serial.println("Sensor SHTC3 listo.");
  }

  // --- VEML7700 ---
  if (!veml.begin()) {
    Serial.println("ERROR: VEML7700 no detectado.");
  } else {
    veml.setGain(VEML7700_GAIN_1);
    veml.setIntegrationTime(VEML7700_IT_100MS);
    veml.powerSaveEnable(true);
    Serial.println("Sensor VEML7700 listo.");
  }

  // --- INA226 ---
  if (!ina.begin()) {
    Serial.println("ERROR: INA226 no detectado.");
  } else {
    // Calibración: shunt = 0.1 ohm, corriente máx esperada = 2 A
    int calibStatus = ina.setMaxCurrentShunt(0.5, 0.1);
    if (calibStatus != INA226_ERR_NONE) {
      Serial.printf("ERROR al calibrar INA226: código 0x%04X\n", calibStatus);
    } else {
      Serial.println("Sensor INA226 listo y calibrado.");
    }
  }
}

void loop() {
  // SHTC3
  sensors_event_t humidity, temperature;
  bool shtc3_ok = shtc3.getEvent(&humidity, &temperature);

  // VEML7700
  float lux = veml.readLux();

  // INA226
  float voltage = ina.getBusVoltage();     // V
  float current = ina.getCurrent();        // A
  float power   = ina.getPower();          // W

  if (shtc3_ok) {
    Serial.printf("Temp: %.2f °C | Humedad: %.2f %%", temperature.temperature, humidity.relative_humidity);
  } else {
    Serial.print("SHTC3 FAIL");
  }

  Serial.printf(" | Luz: %.2f lx", lux);
  Serial.printf(" | V: %.3f V | I: %.3f A | P: %.3f W\n", voltage, current, power);

  delay(10000);  // Espera 10 s
}
