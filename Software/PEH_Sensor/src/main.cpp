#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_ID "NODE_SENSOR"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_ALL_SENSORS_UUID "0000aaaa-0000-1000-8000-00805f9b34fb"
#define CHAR_ACK_UUID "0000aaff-0000-1000-8000-00805f9b34fb"

#define PACKET_SIZE 5
#define MEASURE_CYCLE_MINUTES 5
#define BLE_TIMEOUT_SECONDS 20
#define NUM_REGISTROS 10

// ACTIVAR/DESACTIVAR DEBUG SERIAL
#define DEBUG_SERIAL

#include <Wire.h>
#include <SparkFun_SHTC3.h>
#include <Adafruit_VEML7700.h>
#include <INA226.h>
#include <FS.h>
#include <SPIFFS.h>

#define SPIFFS_PATH "/sensores.dat"

#define SDA_PIN 4
#define SCL_PIN 5
#define A_IN_SKU 6
#define EN_SKU 7

SHTC3 shtc3;
Adafruit_VEML7700 veml = Adafruit_VEML7700();
INA226 ina(0x40, &Wire);

bool shtc3_ok = false;
bool veml_ok = false;
bool ina_ok = false;

struct SensorData
{
  float temp;
  float humAir;
  float humSoil;
  float lux;
  float batt;
};

// BLE
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharAllSensors;
BLECharacteristic *pCharAck;

volatile bool ack_received = false;

// --- BLE CALLBACKS ---
class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
#ifdef DEBUG_SERIAL
    Serial.println("Cliente BLE conectado.");
#endif
  }
  void onDisconnect(BLEServer *pServer)
  {
#ifdef DEBUG_SERIAL
    Serial.println("Cliente BLE desconectado.");
#endif
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
#ifdef DEBUG_SERIAL
      Serial.println("ACK recibido.");
#endif
    }
  }
};

// --- BLE INIT ---
void iniciarBLE()
{
#ifdef DEBUG_SERIAL
  Serial.println("Inicializando BLE...");
#endif

  BLEDevice::init(DEVICE_ID);
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  pService = pServer->createService(SERVICE_UUID);

  pCharAllSensors = pService->createCharacteristic(CHAR_ALL_SENSORS_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  BLE2902 *p2902 = new BLE2902();
  pCharAllSensors->addDescriptor(p2902);

  pCharAck = pService->createCharacteristic(CHAR_ACK_UUID, BLECharacteristic::PROPERTY_WRITE);
  pCharAck->setCallbacks(new AckCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);

  BLEDevice::startAdvertising();
#ifdef DEBUG_SERIAL
  Serial.println("BLE advertising activo.");
#endif
}

void pararBLE()
{
  BLEDevice::deinit(false);
#ifdef DEBUG_SERIAL
  Serial.println("BLE detenido.");
#endif
}

// --- UTILIDAD: desbloquear bus I2C ---
void desbloquearBusI2C()
{
  Wire.end();
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(SCL_PIN, HIGH);
  delay(1);
  for (int i = 0; i < 9; i++)
  {
    digitalWrite(SCL_PIN, LOW);
    delayMicroseconds(5);
    digitalWrite(SCL_PIN, HIGH);
    delayMicroseconds(5);
  }
  pinMode(SDA_PIN, OUTPUT_OPEN_DRAIN);
  digitalWrite(SDA_PIN, LOW);
  delayMicroseconds(5);
  digitalWrite(SDA_PIN, HIGH);
  delayMicroseconds(5);
}

// --- REINTENTO BEGIN ---
template <typename SensorClass>
bool intentarReintentoBegin(SensorClass &sensor, TwoWire *wire = &Wire, int intentos = 3)
{
  for (int i = 0; i < intentos; i++)
  {
    if (sensor.begin(wire))
      return true;
    delay(150);
  }
  return false;
}

// --- SENSORES ---
void iniciarSensores()
{
  delay(100); // Espera tras I2C

  // --- SHTC3 SparkFun ---
  shtc3_ok = (shtc3.begin(Wire) == SHTC3_Status_Nominal);
  if (!shtc3_ok)
  {
#ifdef DEBUG_SERIAL
    Serial.println("SHTC3 no encontrado.");
#endif
  }

  // --- VEML7700 ---
  veml_ok = intentarReintentoBegin(veml);
  if (veml_ok)
  {
    veml.setGain(VEML7700_GAIN_1);
    veml.setIntegrationTime(VEML7700_IT_100MS);
    veml.powerSaveEnable(true);
  }

  // --- INA226 ---
  ina_ok = ina.begin();
  if (ina_ok)
  {
    int status = ina.setMaxCurrentShunt(0.5, 0.1);
    if (status != INA226_ERR_NONE)
    {
#ifdef DEBUG_SERIAL
      Serial.printf("INA226 calibración fallida: código 0x%04X\n", status);
#endif
      ina_ok = false;
    }
  }

#ifdef DEBUG_SERIAL
  Serial.printf("SHTC3: %s | VEML7700: %s | INA226: %s\n",
                shtc3_ok ? "OK" : "FAIL",
                veml_ok ? "OK" : "FAIL",
                ina_ok ? "OK" : "FAIL");
#endif
}

SensorData leerSensores()
{
  SensorData data;

  // --- SHTC3 (SparkFun) ---
  if (shtc3_ok && shtc3.update() == SHTC3_Status_Nominal)
  {
    data.temp = shtc3.toDegC();
    data.humAir = shtc3.toPercent();
  }
  else
  {
    data.temp = -99.0;
    data.humAir = -1.0;
  }

  // --- VEML7700 ---
  data.lux = veml_ok ? veml.readLux() : -1.0;

  // --- INA226 ---
  data.batt = ina_ok ? ina.getBusVoltage() : -1.0;

  // --- Humedad del suelo
  digitalWrite(EN_SKU, 1);
  delay(100);
  data.humSoil = analogRead(A_IN_SKU);
  digitalWrite(EN_SKU, 0);

  return data;
}

int contarRegistrosSPIFFS()
{
  File file = SPIFFS.open(SPIFFS_PATH, FILE_READ);
  if (!file)
    return 0;
  int size = file.size();
  file.close();
  return size / sizeof(SensorData);
}

void borrarArchivoSPIFFS()
{
  SPIFFS.remove(SPIFFS_PATH);
}

bool leerPaqueteSPIFFS(int inicio, int cantidad, SensorData *destino)
{
  File file = SPIFFS.open(SPIFFS_PATH, FILE_READ);
  if (!file)
    return false;
  file.seek(inicio * sizeof(SensorData));
  for (int i = 0; i < cantidad; i++)
  {
    if (file.read((uint8_t *)&destino[i], sizeof(SensorData)) != sizeof(SensorData))
    {
      file.close();
      return false;
    }
  }
  file.close();
  return true;
}

// --- ENVIAR PAQUETES ---
void enviarPaquetesSPIFFS()
{
  int total = contarRegistrosSPIFFS();
#ifdef DEBUG_SERIAL
  Serial.printf("SPIFFS contiene %d registros\n", total);
#endif
  int index = 0;

  while (total > 0)
  {
    int currentPacketSize = min(PACKET_SIZE, total);
    SensorData packet[PACKET_SIZE];

    if (!leerPaqueteSPIFFS(index, currentPacketSize, packet))
    {
#ifdef DEBUG_SERIAL
      Serial.println("Error leyendo paquete desde SPIFFS");
#endif
      return;
    }

    ack_received = false;
    pCharAllSensors->setValue((uint8_t *)packet, currentPacketSize * sizeof(SensorData));
    pCharAllSensors->notify();

    unsigned long ackStartTime = millis();
    while (!ack_received && (millis() - ackStartTime) < 4000)
      delay(10);

    if (ack_received)
    {
#ifdef DEBUG_SERIAL
      Serial.println("[SPIFFS] ACK OK → avanzando...");
#endif
      index += currentPacketSize;
      total -= currentPacketSize;
    }
    else
    {
#ifdef DEBUG_SERIAL
      Serial.println("[SPIFFS] Timeout esperando ACK. Abandonando envío.");
#endif
      return;
    }
  }

  borrarArchivoSPIFFS();
#ifdef DEBUG_SERIAL
  Serial.println("[SPIFFS] Todos los datos enviados y archivo borrado.");
#endif
}

// --- DEEP SLEEP ---
void irDeepSleep()
{
  Wire.end(); // libera bus
  delay(100);
  uint64_t sleep_us = (uint64_t)(MEASURE_CYCLE_MINUTES * 60ULL * 1000000ULL);
#ifdef DEBUG_SERIAL
  Serial.printf("Entrando en deep sleep %.2f minutos (%.0f ms)...\n", MEASURE_CYCLE_MINUTES, sleep_us / 1000.0);
#endif
  esp_sleep_enable_timer_wakeup(sleep_us);
  esp_deep_sleep_start();
}

void guardarEnSPIFFS(const SensorData &data)
{
  File file = SPIFFS.open(SPIFFS_PATH, FILE_APPEND);
  if (!file)
  {
#ifdef DEBUG_SERIAL
    Serial.println("Error abriendo archivo para guardar");
#endif
    return;
  }
  file.write((uint8_t *)&data, sizeof(SensorData));
  file.close();
}

// --- SETUP ---
void setup()
{
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  while (!Serial)
  {
  }
  delay(200);
  Serial.println("--- Ciclo de medida ---");
#endif

  if (!SPIFFS.begin(true))
  {
#ifdef DEBUG_SERIAL
    Serial.println("Error al montar SPIFFS");
#endif
  }

  desbloquearBusI2C();
  Wire.begin(SDA_PIN, SCL_PIN);
  iniciarSensores();

  SensorData data = leerSensores();
  guardarEnSPIFFS(data);
  int count = contarRegistrosSPIFFS();

#ifdef DEBUG_SERIAL
  Serial.printf("Guardada medida → Total en SPIFFS = %d\n", count);
#endif

  if ((count % NUM_REGISTROS) == 0 && count > 0)
  {
#ifdef DEBUG_SERIAL
    Serial.println("Cantidad de registros es múltiplo de " + String(NUM_REGISTROS) + " → intentar enviar BLE.");
#endif

    iniciarBLE();
    unsigned long startTime = millis();
    bool connected = false;

    while ((millis() - startTime) < (BLE_TIMEOUT_SECONDS * 1000))
    {
      if (pServer->getConnectedCount() > 0)
      {
        connected = true;
        break;
      }
      delay(100);
    }

    if (connected)
    {
#ifdef DEBUG_SERIAL
      Serial.println("Conexión BLE establecida.");
      Serial.println("Esperando 1500 ms para que app active notify...");
#endif
      delay(1500);
      enviarPaquetesSPIFFS();
    }
    else
    {
#ifdef DEBUG_SERIAL
      Serial.println("Timeout BLE → no se envía. Guardamos para siguiente intento.");
#endif
    }

    pararBLE();
  }
  else
  {
#ifdef DEBUG_SERIAL
    Serial.println("Cantidad de registros no es múltiplo de " + String(NUM_REGISTROS) + ". Volviendo a dormir.");
#endif
  }

  irDeepSleep();
}

void loop()
{
  // No se usa
}