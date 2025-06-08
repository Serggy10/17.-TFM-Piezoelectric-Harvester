#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_ID "NODE_SENSOR"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_ALL_SENSORS_UUID "0000aaaa-0000-1000-8000-00805f9b34fb"
#define CHAR_ACK_UUID "0000aaff-0000-1000-8000-00805f9b34fb"

BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharAllSensors;
BLECharacteristic *pCharAck;

#define BUFFER_SIZE 10
#define PACKET_SIZE 5

struct SensorData
{
  float temp;
  float humAir;
  float humSoil;
  float lux;
  float batt;
};

SensorData data_buffer[BUFFER_SIZE];
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

  pCharAllSensors = pService->createCharacteristic(CHAR_ALL_SENSORS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  pCharAllSensors->addDescriptor(new BLE2902());

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

void enviarPaquetes()
{
  Serial.println("Enviando paquetes de ALL_SENSORS...");

  for (int i = 0; i < BUFFER_SIZE; i += PACKET_SIZE)
  {
    int currentPacketSize = min(PACKET_SIZE, BUFFER_SIZE - i);
    Serial.printf("[ALL_SENSORS] Paquete [%d - %d]:\n", i, i + currentPacketSize - 1);

    for (int j = 0; j < currentPacketSize; j++)
    {
      Serial.printf("  %2d) Temp=%.1f °C | HumAir=%.1f %% | HumSoil=%.1f %% | Lux=%.1f lx | Batt=%.2f V\n",
                    i + j + 1,
                    data_buffer[i + j].temp,
                    data_buffer[i + j].humAir,
                    data_buffer[i + j].humSoil,
                    data_buffer[i + j].lux,
                    data_buffer[i + j].batt);
    }

    ack_received = false;
    pCharAllSensors->setValue((uint8_t *)(data_buffer + i), currentPacketSize * sizeof(SensorData));
    pCharAllSensors->notify();

    unsigned long ackStartTime = millis();
    while (!ack_received && (millis() - ackStartTime) < 4000)
    {
      delay(10);
    }

    if (ack_received)
    {
      Serial.println("[ALL_SENSORS] ACK OK → enviando siguiente paquete...");
    }
    else
    {
      Serial.println("[ALL_SENSORS] Timeout esperando ACK. Abandonando envío.");
      break;
    }
  }

  Serial.println("[ALL_SENSORS] Envío completo.");
}

void loop()
{
  static unsigned long lastUpdate = 0;
  unsigned long now = millis();

  if (now - lastUpdate > 1500)
  {
    lastUpdate = now;

    SensorData data;
    data.temp = random(180, 300) / 10.0;
    data.humAir = random(300, 800) / 10.0;
    data.humSoil = random(100, 800) / 10.0;
    data.lux = random(0, 1000);
    data.batt = random(370, 420) / 100.0;

    data_buffer[buffer_index] = data;
    buffer_index++;

    Serial.printf("Lectura %d: Temp=%.1f °C | HumAir=%.1f %% | HumSoil=%.1f %% | Lux=%.1f lx | Batt=%.2f V\n",
                  buffer_index, data.temp, data.humAir, data.humSoil, data.lux, data.batt);

    if (buffer_index >= BUFFER_SIZE)
    {
      Serial.println("Buffer lleno. Esperando 500 ms antes de enviar...");
      delay(500);

      enviarPaquetes();

      buffer_index = 0;
      Serial.println("Envío completo. Esperando nuevas lecturas...");
    }
  }
}
