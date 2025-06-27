#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define DEVICE_ID "NODE_SENSOR"
#define SERVICE_UUID "12345678-1234-1234-1234-1234567890ab"
#define CHAR_ALL_SENSORS_UUID "0000aaaa-0000-1000-8000-00805f9b34fb"
#define CHAR_ACK_UUID "0000aaff-0000-1000-8000-00805f9b34fb"

#define BUFFER_SIZE 200
#define PACKET_SIZE 5
#define MEASURE_CYCLE_MINUTES 0.1
#define BLE_TIMEOUT_SECONDS 30
#define NUM_REGISTROS 10

// ACTIVAR/DESACTIVAR DEBUG SERIAL
#define DEBUG_SERIAL

#include <Wire.h>
#include <SparkFun_SHTC3.h>
#include <Adafruit_VEML7700.h>
#include <INA226.h>

#define SDA_PIN 4
#define SCL_PIN 5

// Adafruit_SHTC3 shtc3 = Adafruit_SHTC3();
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

// FIFO circular
RTC_DATA_ATTR SensorData data_buffer[BUFFER_SIZE];
RTC_DATA_ATTR int fifo_head = 0;
RTC_DATA_ATTR int fifo_tail = 0;

// BLE
BLEServer *pServer;
BLEService *pService;
BLECharacteristic *pCharAllSensors;
BLECharacteristic *pCharAck;

volatile bool ack_received = false;

// --- FUNCIONES FIFO ---
int fifo_count()
{
  return (fifo_tail >= fifo_head) ? (fifo_tail - fifo_head) : (BUFFER_SIZE - fifo_head + fifo_tail);
}

void fifo_push(SensorData data)
{
  int next_tail = (fifo_tail + 1) % BUFFER_SIZE;
  if (next_tail == fifo_head)
  {
#ifdef DEBUG_SERIAL
    Serial.println("WARNING: FIFO lleno → sobreescribiendo dato más antiguo");
#endif
    fifo_head = (fifo_head + 1) % BUFFER_SIZE;
  }
  data_buffer[fifo_tail] = data;
  fifo_tail = next_tail;
}

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
  delay(1000); // Espera tras I2C

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

  // --- Humedad del suelo (simulada) ---
  data.humSoil = random(100, 800) / 10.0;

  return data;
}


// --- ENVIAR PAQUETES ---
void enviarPaquetes()
{
  int count = fifo_count();
#ifdef DEBUG_SERIAL
  Serial.printf("FIFO contiene %d registros\n", count);
#endif

  int index = fifo_head;
  while (count > 0)
  {
    int currentPacketSize = min(PACKET_SIZE, count);
#ifdef DEBUG_SERIAL
    Serial.printf("[ALL_SENSORS] Paquete desde index %d (%d elementos)\n", index, currentPacketSize);
#endif

    SensorData packet[PACKET_SIZE];
    for (int i = 0; i < currentPacketSize; i++)
    {
      packet[i] = data_buffer[(index + i) % BUFFER_SIZE];
#ifdef DEBUG_SERIAL
      Serial.printf("  %2d) Temp=%.1f °C | HumAir=%.1f %% | HumSoil=%.1f %% | Lux=%.1f lx | Batt=%.2f V\n",
                    i + 1, packet[i].temp, packet[i].humAir,
                    packet[i].humSoil, packet[i].lux, packet[i].batt);
#endif
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
      Serial.println("[ALL_SENSORS] ACK OK → avanzando FIFO...");
#endif
      fifo_head = (fifo_head + currentPacketSize) % BUFFER_SIZE;
      count -= currentPacketSize;
      index = fifo_head;
    }
    else
    {
#ifdef DEBUG_SERIAL
      Serial.println("[ALL_SENSORS] Timeout esperando ACK. Abandonando envío.");
#endif
      break;
    }
  }
#ifdef DEBUG_SERIAL
  Serial.println("[ALL_SENSORS] Envío completo.");
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

// --- SETUP ---
void setup()
{
#ifdef DEBUG_SERIAL
  Serial.begin(115200);
  while (!Serial)
  {
  }
  delay(2000);
  Serial.println("--- Ciclo de medida ---");
#endif

  desbloquearBusI2C();
  Wire.begin(SDA_PIN, SCL_PIN);
  iniciarSensores();

  SensorData data = leerSensores();
  fifo_push(data);

#ifdef DEBUG_SERIAL
  Serial.printf("Guardada medida → FIFO count = %d\n", fifo_count());
#endif

  if ((fifo_count() % NUM_REGISTROS) == 0)
  {
#ifdef DEBUG_SERIAL
    Serial.println("FIFO múltiplo de " + String(NUM_REGISTROS) + "→ intentar enviar BLE.");
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
      enviarPaquetes();
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
    Serial.println("No es múltiplo de " + String(NUM_REGISTROS) + ". Volviendo a dormir.");
#endif
  }

  irDeepSleep();
}

void loop()
{
  // No se usa
}
