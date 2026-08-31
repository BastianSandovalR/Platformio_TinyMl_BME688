#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

#define I2C_SDA_BME 4 // pin DATOS con sensor bosh
#define I2C_SCL_BME 5  // pin RELOJ conexion con sensor bosh
#define I2C_SDA_OLED 17 // pin DATOS conexion con la patnalla oled 
#define I2C_SCL_OLED 18 // pin RELOJ conexion con el pantalla oled
#define OLED_RST 21 // pin RESET FISICO DE la patnalla oled
#define VEXT_PIN 36 // pin CONTROL energia externa alimentar pantalla LED
#define LORA_NSS 8  // conexion lora
#define LORA_DIO1 14 // conexion lora
#define LORA_NRST 12 // conexion lora
#define LORA_BUSY 13 // conexion lora

Adafruit_BME680 bme; 
Adafruit_SSD1306 display(128, 64, &Wire1, OLED_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

// Temporizador síncrono estricto para asegurar los 1.5 segundos exactos
unsigned long tiempo_anterior = 0; // se usan de este tipo para que cuando este numero se llene por la cantidad de bits al hacer la resta no me de un numero negativo 
                                  // y asi pueda ejecutarse el programa por anios

const unsigned long INTERVALO_MUESTREO = 1500; // incrustamos el 1500 en la memoria flash, asi optimizamos nuestro codigo y que esa variable no ocupe espacio en la RAM

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
  
  // Calentador fijo e ininterrumpido para la fase de captura del dataset
  bme.setGasHeater(320, 150); 
  delay(2000);

  tiempo_anterior = millis(); // tiempo desde que se inicia el sistema, es como un odometro de los autos
}
void loop() {
  unsigned long tiempo_actual = millis();

  if (tiempo_actual - tiempo_anterior >= INTERVALO_MUESTREO) {
    tiempo_anterior = tiempo_actual;

    // 1. Pulso Táctico: 320 °C por 150 ms
    bme.setGasHeater(320, 150);
    if (!bme.performReading()) return;

    float temp = bme.temperature;
    float hum = bme.humidity;
    float pres = bme.pressure / 100.0; 
    float gas_res = bme.gas_resistance;
    
    // Transmisión inmediata vía LoRa (Bloqueante)
    String payload = String(temp, 2) + "," + String(hum, 2) + "," + String(pres, 2) + "," + String(gas_res, 2);
    radio.transmit(payload);

    // Actualización de la Pantalla OLED (Bloqueante)
    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.print("IgnisEdge Logger: 1.5s");
    display.drawLine(0, 10, 128, 10, WHITE);
    display.setCursor(0, 20);
    display.print("Gas: "); 
    display.print(gas_res / 1000.0, 1); display.println(" kOhms");
    display.setCursor(0, 35);
    display.print("Temp: "); display.print(temp, 1); display.println(" C");
    display.print("Hum:  "); display.print(hum, 1);  display.println(" %");
    display.display();

    // 2. Cálculo Dinámico de la Lectura en Sombra
    // Guardamos el tiempo gastado directamente como long con signo
    long tiempo_gastado = millis() - tiempo_actual; 
    
    // Casteamos la constante a long para permitir resultados matemáticos negativos
    long tiempo_sombra = (long)INTERVALO_MUESTREO - tiempo_gastado - 10; 

    if (tiempo_sombra > 0) {
        bme.setGasHeater(200, tiempo_sombra);
        bme.performReading(); 
    }
    }
  }