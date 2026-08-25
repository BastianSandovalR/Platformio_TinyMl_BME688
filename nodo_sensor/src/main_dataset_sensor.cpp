#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

#define I2C_SDA_BME 4
#define I2C_SCL_BME 5
#define I2C_SDA_OLED 17
#define I2C_SCL_OLED 18
#define OLED_RST 21
#define VEXT_PIN 36
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_NRST 12
#define LORA_BUSY 13

Adafruit_BME680 bme; 
Adafruit_SSD1306 display(128, 64, &Wire1, OLED_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

void setup() {
  Serial.begin(115200);
  delay(3000); 

  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(50);
  
  Wire1.begin(I2C_SDA_OLED, I2C_SCL_OLED);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  radio.begin(915.0);

  Wire.begin(I2C_SDA_BME, I2C_SCL_BME);
  if (!bme.begin(0x76)) {
    while (true);
  }
  
  // Calentador simple: 320°C por solo 150ms
  bme.setGasHeater(320, 150); 
  delay(2000);
}

void loop() {
  if (!bme.performReading()) {
    return;
  }

  // Capturamos los 4 datos
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pres = bme.pressure / 100.0; 
  float gas_res = bme.gas_resistance;
  
  // Armamos el texto corto y ligero (2 decimales para precisión)
  String payload = String(temp, 2) + "," + String(hum, 2) + "," + String(pres, 2) + "," + String(gas_res, 2);

  // Transmitimos
  radio.transmit(payload);

  // Pantalla
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(1);
  display.print("Lectura Simple OK");
  display.drawLine(0, 10, 128, 10, WHITE);
  display.setCursor(0, 20);
  display.print("Gas: "); 
  display.print(gas_res / 1000.0, 1); display.println(" kOhms");
  display.display();
  
  delay(1000); 
}