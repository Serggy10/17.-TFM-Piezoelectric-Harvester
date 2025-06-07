// #include <Arduino.h>
// #include <BLEDevice.h>
// #include <BLEServer.h>
// #include <BLEUtils.h>
// #include <BLE2902.h>
// #include <ArduinoJson.h>

// #define DEVICE_ID "NODE_01"
// #define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
// #define CHAR_JSON_UUID "12345678-1234-1234-1234-1234567890b1"

// #define DEEP_SLEEP_MINUTES 0.5  // 30 segundos
// #define MAX_CYCLES 10  // 10 ciclos * 30 s = 5 min

// RTC_DATA_ATTR int cycleCounter = 0;
// RTC_DATA_ATTR char jsonBuffer[4096] = "{}";


// void setup() {
//   Serial.begin(115200);
//   while (!Serial);

//   Serial.printf("Ciclo #%d\n", cycleCounter + 1);

//   // Cargar JSON actual
//   DynamicJsonDocument doc(4096);
//   deserializeJson(doc, jsonBuffer);
//   JsonArray readings;

//   if (cycleCounter == 0) {
//     readings = doc.createNestedArray("readings");
//   } else {
//     readings = doc["readings"].as<JsonArray>();
//   }

//   // Simulación de sensores
//   float temp = random(180, 300) / 10.0;
//   float hum_air = random(300, 800) / 10.0;
//   float hum_soil = random(100, 800) / 10.0;
//   int lux = random(0, 1000);
//   float batt_volt = random(370, 420) / 100.0;

//   JsonObject entry = readings.createNestedObject();
//   entry["timestamp"] = millis() / 1000;
//   entry["temp"] = temp;
//   entry["hum_air"] = hum_air;
//   entry["hum_soil"] = hum_soil;
//   entry["lux"] = lux;
//   entry["batt_volt"] = batt_volt;

//   // Guardar JSON actualizado
//   serializeJson(doc, jsonBuffer);

//   Serial.println("Lectura añadida.");
//   Serial.printf("Lecturas acumuladas: %d/%d\n", cycleCounter + 1, MAX_CYCLES);

//   // ¿Enviar?
//   if (cycleCounter + 1 >= MAX_CYCLES) {
//     Serial.println("Enviando JSON por BLE...");

//     // Inicializar BLE
//     BLEDevice::init(DEVICE_ID);
//     BLEServer *pServer = BLEDevice::createServer();
//     BLEService *pService = pServer->createService(SERVICE_UUID);

//     BLECharacteristic *pCharJson = pService->createCharacteristic(
//                                       CHAR_JSON_UUID,
//                                       BLECharacteristic::PROPERTY_READ
//                                    );

//     pCharJson->addDescriptor(new BLE2902());

//     pService->start();

//     BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
//     pAdvertising->addServiceUUID(SERVICE_UUID);
//     pAdvertising->setScanResponse(true);
//     pAdvertising->setMinPreferred(0x06);
//     pAdvertising->setMinPreferred(0x12);

//     BLEDevice::startAdvertising();

//     // Poner el JSON completo en la característica
//     pCharJson->setValue(jsonBuffer);

//     Serial.println("BLE advertising activo. Esperando conexión para leer JSON.");
//     Serial.println(jsonBuffer);

//     // Mantener BLE activo durante 30 s
//     delay(30000);

//     BLEDevice::stopAdvertising();
//     BLEDevice::deinit();

//     Serial.println("BLE terminado. Reseteando contador y buffer.");

//     // Reset
//     cycleCounter = 0;
//     strcpy(jsonBuffer, "{}");
//   } else {
//     cycleCounter++;
//   }

//   // Deep sleep
//   Serial.printf("Entrando en deep sleep durante %.1f minutos...\n", DEEP_SLEEP_MINUTES);
//   Serial.flush();

//   esp_sleep_enable_timer_wakeup(DEEP_SLEEP_MINUTES * 60 * 1000000ULL);
//   esp_deep_sleep_start();
// }

// void loop() {
//   // No se usa
// }


#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>

#define DEVICE_ID "NODE_01"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_JSON_UUID "12345678-1234-1234-1234-1234567890b1"

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharJson;

DynamicJsonDocument doc(4096);
char jsonBuffer[4096] = "{}";

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Inicializando BLE...");

  // Inicializar BLE
  BLEDevice::init(DEVICE_ID);
  pServer = BLEDevice::createServer();
  pService = pServer->createService(SERVICE_UUID);

  pCharJson = pService->createCharacteristic(
                  CHAR_JSON_UUID,
                  BLECharacteristic::PROPERTY_READ
              );

  pCharJson->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.println("BLE advertising activo.");
}

void loop() {
  // Simulación de sensores cada 15 s
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate > 15000) {  // cada 15 s
    lastUpdate = now;

    // Añadir nueva lectura al JSON
    JsonArray readings;

    // Si el doc está vacío, inicializar el array
    if (!doc.containsKey("readings")) {
      readings = doc.createNestedArray("readings");
    } else {
      readings = doc["readings"].as<JsonArray>();
    }

    // Simulación de sensores
    float temp = random(180, 300) / 10.0;
    float hum_air = random(300, 800) / 10.0;
    float hum_soil = random(100, 800) / 10.0;
    int lux = random(0, 1000);
    float batt_volt = random(370, 420) / 100.0;

    JsonObject entry = readings.createNestedObject();
    entry["timestamp"] = now / 1000;
    entry["temp"] = temp;
    entry["hum_air"] = hum_air;
    entry["hum_soil"] = hum_soil;
    entry["lux"] = lux;
    entry["batt_volt"] = batt_volt;

    // Guardar JSON actualizado
    serializeJson(doc, jsonBuffer);

    // Actualizar la característica
    pCharJson->setValue(jsonBuffer);

    // Log
    Serial.println("Nueva lectura añadida al JSON:");
    Serial.println(jsonBuffer);
  }
}
