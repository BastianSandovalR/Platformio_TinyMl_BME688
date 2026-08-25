#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

// Incluimos nuestro modelo de Machine Learning (Destilado)
#include "IgnisEdge_Model.h"

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

// ==========================================
// PARAMETROS DEL STANDARD SCALER (De Python)
// ==========================================
const float MEAN_TEMP = 10.1643;
const float STD_TEMP  = 4.1954;
const float MEAN_HUM  = 66.2074;
const float STD_HUM   = 9.8907;
const float MEAN_PRES = 1003.5219;
const float STD_PRES  = 1.1891;
const float MEAN_GAS  = 121828.0204;
const float STD_GAS   = 20025.0210;

// ==========================================
// LÓGICA DE CONFIRMACIÓN DE ALERTAS
// ==========================================
int contador_alertas = 0;
const int UMBRAL_ALERTAS = 3; // Necesita 3 lecturas seguidas de anomalía para disparar

void setup() {
  Serial.begin(115200);
  delay(3000); 

  // 1. Encender energía de los periféricos
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, LOW); 
  delay(50);
  
  // 2. Iniciar Pantalla
  Wire1.begin(I2C_SDA_OLED, I2C_SCL_OLED);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  
  display.println("Arrancando IgnisEdge");
  display.display();
  delay(1000);
  
  // 3. Iniciar LoRa
  display.print("LoRa: ");
  if (radio.begin(915.0) == RADIOLIB_ERR_NONE) {
    display.println("OK");
    Serial.println("LoRa OK");
  } else {
    display.println("ERROR");
  }
  display.display();
  delay(1000);

  // 4. Iniciar Sensor BME688
  display.print("BME688: ");
  display.display();
  Wire.begin(I2C_SDA_BME, I2C_SCL_BME);
  
  // OJO: Si falla aquí, intenta cambiar 0x76 por 0x77
  if (!bme.begin(0x76)) {
    display.println("ERROR");
    display.display();
    Serial.println("Error BME688");
    while (true); // Si se congela, verás "BME688: ERROR" en pantalla
  }
  display.println("OK");
  display.display();
  
  // Calentador para VOCs
  bme.setGasHeater(320, 150); 
  delay(2000);

  // Todo listo
  display.clearDisplay();
  display.setCursor(0,0);
  display.println("IGNISEDGE NODO AI");
  display.println("Iniciando Inferencia...");
  display.display();
  delay(2000);
}

void loop() {
  if (!bme.performReading()) {
    return;
  }

  // 1. Capturar Variables Crudas
  float temp = bme.temperature;
  float hum = bme.humidity;
  float pres = bme.pressure / 100.0; 
  float gas_res = bme.gas_resistance;

  // 2. Escalar Datos (Misma matematica que en Python)
  float temp_scaled = (temp - MEAN_TEMP) / STD_TEMP;
  float hum_scaled  = (hum - MEAN_HUM) / STD_HUM;
  float pres_scaled = (pres - MEAN_PRES) / STD_PRES;
  float gas_scaled  = (gas_res - MEAN_GAS) / STD_GAS;

  // Creamos el arreglo que espera m2cgen
  double features_array[] = {temp_scaled, hum_scaled, pres_scaled, gas_scaled};

  // 3. INFERENCIA CON EL MODELO (Destilación)
  double anomaly_score = score(features_array); // Función generada en IgnisEdge_Model.h

  // 4. Lógica de Decisión (Frontera en 0)
  display.clearDisplay();
  display.setCursor(0,0);
  
  if (anomaly_score < 0.0) {
    contador_alertas++; // Sumamos una posible detección
    display.println("? ANALIZANDO HUMO...");
    display.print("Confirmacion: "); display.print(contador_alertas); display.println("/3");
    
    if (contador_alertas >= UMBRAL_ALERTAS) {
      // ESTADO DE ALERTA CONFIRMADA
      String payload = "ALERTA_FUEGO! Score: " + String(anomaly_score, 4) + ", Gas: " + String(gas_res/1000.0, 1) + "kOhm";
      radio.transmit(payload);
      
      Serial.println("🔥 FUEGO DETECTADO Y CONFIRMADO - Trama enviada.");
      display.setCursor(0, 30);
      display.println("! TRANSMITIENDO !");
    }
  } else {
    // ESTADO NORMAL
    contador_alertas = 0; // Reseteamos el contador porque entró aire limpio
    Serial.println("Aire Limpio. Sin transmisión.");
    display.println("AIRE LIMPIO");
  }
  
  display.setCursor(0, 50);
  display.print("Score: "); display.println(String(anomaly_score, 4));
  display.display();

  // ==========================================
  // CONSOLA DE DIAGNÓSTICO (Ver en PC)
  // ==========================================
  Serial.println("--- DIAGNÓSTICO DE INFERENCIA ---");
  Serial.print("1. Temp Bruta: "); Serial.print(temp); Serial.print(" C | Escalada: "); Serial.println(temp_scaled);
  Serial.print("2. Hum  Bruta: "); Serial.print(hum); Serial.print(" % | Escalada: "); Serial.println(hum_scaled);
  Serial.print("3. Pres Bruta: "); Serial.print(pres); Serial.print(" hPa | Escalada: "); Serial.println(pres_scaled);
  Serial.print("4. Gas  Bruto: "); Serial.print(gas_res); Serial.print(" Ohms | Escalada: "); Serial.println(gas_scaled);
  Serial.print("=> SCORE IA: "); Serial.println(anomaly_score, 6);
  Serial.println("---------------------------------");
  
  // Frecuencia de muestreo
  delay(1000); 
}