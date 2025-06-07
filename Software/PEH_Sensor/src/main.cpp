#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_ID "NODE_SENSOR"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"

#define CHAR_TEMP_UUID "0000aaa1-0000-1000-8000-00805f9b34fb"
#define CHAR_HUM_AIR_UUID "0000aaa2-0000-1000-8000-00805f9b34fb"
#define CHAR_HUM_SOIL_UUID "0000aaa3-0000-1000-8000-00805f9b34fb"
#define CHAR_LUX_UUID "0000aaa4-0000-1000-8000-00805f9b34fb"
#define CHAR_BATT_UUID "0000aaa5-0000-1000-8000-00805f9b34fb"
#define CHAR_ACK_UUID "0000aaff-0000-1000-8000-00805f9b34fb"

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharTemp;
BLECharacteristic *pCharHumAir;
BLECharacteristic *pCharHumSoil;
BLECharacteristic *pCharLux;
BLECharacteristic *pCharBatt;
BLECharacteristic *pCharAck;

#define BUFFER_SIZE 10
#define PACKET_SIZE 5

float temp_buffer[BUFFER_SIZE];
float hum_air_buffer[BUFFER_SIZE];
float hum_soil_buffer[BUFFER_SIZE];
float lux_buffer[BUFFER_SIZE];
float batt_buffer[BUFFER_SIZE];
int buffer_index = 0;

volatile bool ack_received = false;

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Cliente BLE conectado.");
  }
  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Cliente BLE desconectado. Reiniciando advertising...");
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->stop();
    delay(50);
    pAdvertising->start();
  }
};

class AckCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic)
  {
    std::string value = pCharacteristic->getValue();
    if (value == "OK")
    {
      ack_received = true;
      Serial.println("ACK recibido.");
    }
  }
};

void setup()
{
  Serial.begin(115200);
  BLEDevice::init(DEVICE_ID);

  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  pService = pServer->createService(SERVICE_UUID);

  pCharTemp = pService->createCharacteristic(CHAR_TEMP_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharTemp->addDescriptor(new BLE2902());
  pCharHumAir = pService->createCharacteristic(CHAR_HUM_AIR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharHumAir->addDescriptor(new BLE2902());
  pCharHumSoil = pService->createCharacteristic(CHAR_HUM_SOIL_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharHumSoil->addDescriptor(new BLE2902());
  pCharLux = pService->createCharacteristic(CHAR_LUX_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharLux->addDescriptor(new BLE2902());
  pCharBatt = pService->createCharacteristic(CHAR_BATT_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharBatt->addDescriptor(new BLE2902());
  pCharAck = pService->createCharacteristic(CHAR_ACK_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharAck->setCallbacks(new AckCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE advertising activo.");
}

void enviarPaquetes(BLECharacteristic *pChar, float *buffer, const char *etiqueta)
{
  Serial.printf("Enviando paquetes de %s...\n", etiqueta);

  for (int i = 0; i < BUFFER_SIZE; i += PACKET_SIZE)
  {
    int currentPacketSize = min(PACKET_SIZE, BUFFER_SIZE - i);
    Serial.printf("[%s] Paquete [%d - %d]: ", etiqueta, i, i + currentPacketSize - 1);
    for (int j = 0; j < currentPacketSize; j++)
    {
      Serial.printf("%.1f ", buffer[i + j]);
    }
    Serial.println();

    ack_received = false;
    pChar->setValue((uint8_t *)(buffer + i), currentPacketSize * sizeof(float));
    pChar->notify();

    unsigned long ackStartTime = millis();
    while (!ack_received && (millis() - ackStartTime) < 4000)
    {
      delay(10);
    }

    if (ack_received)
    {
      Serial.printf("[%s] ACK OK → enviando siguiente paquete...\n", etiqueta);
    }
    else
    {
      Serial.printf("[%s] Timeout esperando ACK. Abandonando envío.\n", etiqueta);
      break;
    }
  }

  Serial.printf("[%s] Envío completo.\n", etiqueta);
}

void loop()
{
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate > 1500)
  {
    lastUpdate = now;

    float temp = random(180, 300) / 10.0;
    float hum_air = random(300, 800) / 10.0;
    float hum_soil = random(100, 800) / 10.0;
    float lux = random(0, 1000);
    float batt = random(370, 420) / 100.0;

    temp_buffer[buffer_index] = temp;
    hum_air_buffer[buffer_index] = hum_air;
    hum_soil_buffer[buffer_index] = hum_soil;
    lux_buffer[buffer_index] = lux;
    batt_buffer[buffer_index] = batt;
    buffer_index++;

    Serial.printf("Lectura %d: Temp = %.1f °C | HumAir = %.1f %% | HumSoil = %.1f %% | Lux = %.1f lx | Batt = %.2f V\n",
                  buffer_index, temp, hum_air, hum_soil, lux, batt);

    if (buffer_index >= BUFFER_SIZE)
    {
      Serial.println("Buffer lleno. Esperando 500 ms antes de enviar...");
      delay(500);

      enviarPaquetes(pCharTemp, temp_buffer, "TEMP");
      delay(500);
      enviarPaquetes(pCharHumAir, hum_air_buffer, "HUM_AIR");
      delay(500);
      enviarPaquetes(pCharHumSoil, hum_soil_buffer, "HUM_SOIL");
      delay(500);
      enviarPaquetes(pCharLux, lux_buffer, "LUX");
      delay(500);
      enviarPaquetes(pCharBatt, batt_buffer, "BATT");
      delay(500);

      buffer_index = 0;
      Serial.println("Envío completo. Esperando nuevas lecturas...");
    }
  }
}