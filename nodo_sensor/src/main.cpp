#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>
#include "mahalanobis_model.h" // Tu modelo de Python transpilado

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

// Identificador topológico
const String NODO_ID = "NODO_01";

Adafruit_BME680 bme; 
Adafruit_SSD1306 display(128, 64, &Wire1, OLED_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

// Temporizador
unsigned long tiempo_anterior = 0; 
const unsigned long INTERVALO_MUESTREO = 1500;
const unsigned long TIEMPO_BURN_IN = 15 * 60 * 1000; // 15 minutos en ms

// ==========================================
// VARIABLES DE INGENIERÍA DE CARACTERÍSTICAS
// ==========================================
float temp_ema = -1.0;
float hum_ema = -1.0;
float gas_ema = -1.0;

// Buffers circulares para 10 periodos (~15 segundos)
const int WINDOW_SIZE = 10;
float gas_buffer[WINDOW_SIZE] = {0};
float temp_buffer[WINDOW_SIZE] = {0};
float hum_buffer[WINDOW_SIZE] = {0};
int buffer_idx = 0;
bool buffer_filled = false;

// Estado Lógico
float leaky_bucket = 0.0;

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
    Serial.println("Error BME688");
    while (true);
  }
  
  bme.setGasHeater(320, 150); 
  delay(2000);

  tiempo_anterior = millis(); 
}

void loop() {
  unsigned long tiempo_actual = millis();

  if (tiempo_actual - tiempo_anterior >= INTERVALO_MUESTREO) {
    tiempo_anterior = tiempo_actual;

    // 1. LECTURA DEL SENSOR
    bme.setGasHeater(320, 150);
    if (!bme.performReading()) return;

    float temp = bme.temperature;
    float hum = bme.humidity;
    float gas_res = bme.gas_resistance;

    // 2. EXTRACCIÓN DE CARACTERÍSTICAS "AL VUELO" (Reemplazo de Pandas)
    if (temp_ema == -1.0) { // Inicialización en el primer ciclo
        temp_ema = temp;
        hum_ema = hum;
        gas_ema = gas_res;
    } else {
        temp_ema = (temp * 0.01) + (temp_ema * 0.99);
        hum_ema = (hum * 0.01) + (hum_ema * 0.99);
        gas_ema = (gas_res * 0.10) + (gas_ema * 0.90);
    }

    // Leemos los valores antiguos de t-10 antes de sobrescribir el buffer
    float old_temp = temp_buffer[buffer_idx];
    float old_hum = hum_buffer[buffer_idx];
    float old_gas = gas_buffer[buffer_idx];

    // Actualizamos el buffer circular
    temp_buffer[buffer_idx] = temp_ema;
    hum_buffer[buffer_idx] = hum_ema;
    gas_buffer[buffer_idx] = gas_ema;

    if (buffer_filled) {
        // Cálculos de Deltas
        float delta_temp = temp_ema - old_temp;
        float delta_hum = hum_ema - old_hum;
        float delta_gas_pct = ((gas_ema - old_gas) / old_gas) * 100.0;

        // Cálculo de Varianza de la ventana
        float mean_gas = 0;
        for(int i=0; i<WINDOW_SIZE; i++) mean_gas += gas_buffer[i];
        mean_gas /= WINDOW_SIZE;
        
        float var_gas = 0;
        for(int i=0; i<WINDOW_SIZE; i++) {
            var_gas += pow(gas_buffer[i] - mean_gas, 2);
        }
        var_gas /= (WINDOW_SIZE - 1); // Varianza muestral

        // 3. INFERENCIA MATEMÁTICA: DISTANCIA DE MAHALANOBIS
        // Vector X: ['Gas_EMA', 'Delta_Temp', 'Delta_Hum', 'Delta_Gas_%', 'Varianza_Gas']
        float X[5] = {gas_ema, delta_temp, delta_hum, delta_gas_pct, var_gas};
        float X_scaled[5];
        float delta_X[5];

        // Estandarización y resta de Mu
        for(int i=0; i<5; i++) {
            X_scaled[i] = (X[i] - scaler_mean[i]) / scaler_std[i];
            delta_X[i] = X_scaled[i] - mu_mahal[i];
        }

        // Multiplicación matricial: D^2 = delta_X^T * inv_cov_matrix * delta_X
        float distancia_cuadrada = 0;
        for(int i=0; i<5; i++) {
            float row_sum = 0;
            for(int j=0; j<5; j++) {
                row_sum += delta_X[j] * inv_cov_matrix[j][i];
            }
            distancia_cuadrada += row_sum * delta_X[i];
        }
        float distancia = sqrt(distancia_cuadrada);

        // 4. LÓGICA DE CONTROL: FILTRO ASIMÉTRICO Y LEAKY BUCKET
        // 4. LÓGICA DE CONTROL: FILTRO ASIMÉTRICO Y LEAKY BUCKET
        if (tiempo_actual < TIEMPO_BURN_IN) {
            leaky_bucket = 0; // Ceguera táctica por microcalentador
        } else {
            if (distancia > UMBRAL_MAHALANOBIS && delta_gas_pct < 0) {
                leaky_bucket += 1.5;
            } else {
                leaky_bucket -= 0.2;
                if (leaky_bucket < 0) leaky_bucket = 0;
            }

            // 5. DECISIÓN DE TRANSMISIÓN LORA (Silencio hasta la alerta)
            if (leaky_bucket >= 25.0) {
                String payload = NODO_ID + ",ALERTA," + String(leaky_bucket, 1) + "," + String(gas_res, 0);
                radio.transmit(payload);
                Serial.println("¡ALERTA TRANSMITIDA!: " + payload);
            }
        }

        // 6. INTERFAZ OLED (Orden de buffer corregido)
        display.clearDisplay();
        display.setCursor(0,0);
        display.print("IgnisEdge - NODO 01");
        display.drawLine(0, 10, 128, 10, WHITE);
        
        display.setCursor(0, 20);
        display.print("Bckt: "); display.print(leaky_bucket, 1); display.println("/25.0");
        display.print("Dist: "); display.println(distancia, 2);
        display.print("dGas: "); display.print(delta_gas_pct, 2); display.println(" %");

        // Imprimir Estado al final
        display.setCursor(0, 50);
        if (tiempo_actual < TIEMPO_BURN_IN) {
            display.print("ESTADO: CALIBRANDO...");
        } else if (leaky_bucket >= 25.0) {
            display.print("ESTADO: FUEGO!!!!!   ");
        } else {
            display.print("ESTADO: SEGURO       ");
        }
        
        display.display();
    }

    // Avanzar índice del buffer
    buffer_idx = (buffer_idx + 1) % WINDOW_SIZE;
    if (buffer_idx == 0) buffer_filled = true;

    // 6. CÁLCULO DINÁMICO DE SOMBRA
    long tiempo_gastado = millis() - tiempo_actual; 
    long tiempo_sombra = (long)INTERVALO_MUESTREO - tiempo_gastado - 10; 

    if (tiempo_sombra > 0) {
        bme.setGasHeater(200, tiempo_sombra);
        bme.performReading(); 
    }
  }
}