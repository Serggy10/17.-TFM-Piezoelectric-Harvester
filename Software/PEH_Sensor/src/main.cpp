#include <Arduino.h>

// put function declarations here:
void setup() {
  Serial.begin(115200);
  while (!Serial) {
    ; // espera a que el puerto USB CDC esté listo
  }
  Serial.println("ESP32-S3 USB CDC funcionando!");
}

void loop() {
  Serial.println("Contando...");
  delay(1000);
}
