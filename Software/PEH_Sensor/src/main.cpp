#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_ID "NODE_01"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"

// UUIDs de las características
#define CHAR_TEMP_UUID     "12345678-1234-1234-1234-1234567890c1"
#define CHAR_HUM_AIR_UUID  "12345678-1234-1234-1234-1234567890c2"
#define CHAR_HUM_SOIL_UUID "12345678-1234-1234-1234-1234567890c3"
#define CHAR_LUX_UUID      "12345678-1234-1234-1234-1234567890c4"
#define CHAR_BATT_UUID     "12345678-1234-1234-1234-1234567890c5"

// Características BLE
BLEServer *pServer;
// Callback para saber cuándo se conecta / desconecta un cliente BLE
class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      Serial.println("Cliente BLE conectado.");
    };

void onDisconnect(BLEServer* pServer) {
    Serial.println("Cliente BLE desconectado. Reiniciando advertising completo...");

    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();  // <--- MUY IMPORTANTE: parar primero
    delay(50);             // (pequeño delay para asegurar limpieza en algunos stacks)
    pAdvertising->start(); // <--- volver a iniciar
}
};


BLEService *pService;
BLECharacteristic *pCharTemp;
BLECharacteristic *pCharHumAir;
BLECharacteristic *pCharHumSoil;
BLECharacteristic *pCharLux;
BLECharacteristic *pCharBatt;

// Variables de registro (ejemplo: últimos N valores)
#define MAX_READINGS 100
float temp_log[MAX_READINGS];
float hum_air_log[MAX_READINGS];
float hum_soil_log[MAX_READINGS];
int lux_log[MAX_READINGS];
float batt_volt_log[MAX_READINGS];
int reading_index = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("Inicializando BLE...");

  BLEDevice::init(DEVICE_ID);
pServer = BLEDevice::createServer();
pServer->setCallbacks(new MyServerCallbacks());  // <--- ESTO AÑADIMOS

pService = pServer->createService(SERVICE_UUID);
  // Crear cada característica
  pCharTemp = pService->createCharacteristic(
                  CHAR_TEMP_UUID,
                  BLECharacteristic::PROPERTY_READ
              );
  pCharTemp->addDescriptor(new BLE2902());

  pCharHumAir = pService->createCharacteristic(
                  CHAR_HUM_AIR_UUID,
                  BLECharacteristic::PROPERTY_READ
              );
  pCharHumAir->addDescriptor(new BLE2902());

  pCharHumSoil = pService->createCharacteristic(
                  CHAR_HUM_SOIL_UUID,
                  BLECharacteristic::PROPERTY_READ
              );
  pCharHumSoil->addDescriptor(new BLE2902());

  pCharLux = pService->createCharacteristic(
                  CHAR_LUX_UUID,
                  BLECharacteristic::PROPERTY_READ
              );
  pCharLux->addDescriptor(new BLE2902());

  pCharBatt = pService->createCharacteristic(
                  CHAR_BATT_UUID,
                  BLECharacteristic::PROPERTY_READ
              );
  pCharBatt->addDescriptor(new BLE2902());

  // Iniciar servicio y advertising
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
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate > 1500) {  // cada 15 s
    lastUpdate = now;

    // Simulación de sensores
    float temp = random(180, 300) / 10.0;
    float hum_air = random(300, 800) / 10.0;
    float hum_soil = random(100, 800) / 10.0;
    int lux = random(0, 1000);
    float batt_volt = random(370, 420) / 100.0;

    // Guardar en arrays circulares
    if (reading_index < MAX_READINGS) {
      temp_log[reading_index] = temp;
      hum_air_log[reading_index] = hum_air;
      hum_soil_log[reading_index] = hum_soil;
      lux_log[reading_index] = lux;
      batt_volt_log[reading_index] = batt_volt;
      reading_index++;
    } else {
      Serial.println("Buffer de lecturas lleno. Se podrían hacer estrategias de compresión / envío parcial.");
    }

    // Actualizar características con la última lectura
    pCharTemp->setValue((uint8_t*)&temp, sizeof(temp));
    pCharHumAir->setValue((uint8_t*)&hum_air, sizeof(hum_air));
    pCharHumSoil->setValue((uint8_t*)&hum_soil, sizeof(hum_soil));
    pCharLux->setValue((uint8_t*)&lux, sizeof(lux));
    pCharBatt->setValue((uint8_t*)&batt_volt, sizeof(batt_volt));

    // Log
    Serial.println("Nueva lectura:");
    Serial.printf("Temp: %.1f °C\n", temp);
    Serial.printf("Hum Air: %.1f %%\n", hum_air);
    Serial.printf("Hum Soil: %.1f %%\n", hum_soil);
    Serial.printf("Lux: %d lx\n", lux);
    Serial.printf("Batt Volt: %.2f V\n", batt_volt);
    Serial.printf("Lecturas acumuladas: %d\n", reading_index);
  }
}
