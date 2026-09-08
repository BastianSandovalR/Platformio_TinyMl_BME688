#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RadioLib.h>

#define I2C_SDA_OLED 17
#define I2C_SCL_OLED 18
#define OLED_RST 21
#define VEXT_PIN 36
#define LORA_NSS 8
#define LORA_DIO1 14
#define LORA_NRST 12
#define LORA_BUSY 13

Adafruit_SSD1306 display(128, 64, &Wire1, OLED_RST);
SX1262 radio = new Module(LORA_NSS, LORA_DIO1, LORA_NRST, LORA_BUSY);

// Bandera lógica de interrupción en RAM
volatile bool paqueteRecibido = false; 

ICACHE_RAM_ATTR void setFlag(void) { 
  paqueteRecibido = true;            
}

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
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println("IGNISEDGE GATEWAY");
  display.drawLine(0, 10, 128, 10, WHITE);
  display.setCursor(0, 20);
  display.println("Estado: Escuchando...");
  display.display();

  // Inicializar radio LoRa
  int state = radio.begin(915.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("{\"sistema\": \"Gateway_Iniciado\", \"status\": \"OK\"}");
  } else {
    Serial.print("{\"sistema\": \"Gateway_Error\", \"codigo\": ");
    Serial.print(state);
    Serial.println("}");
    while (true);
  }

  // Configurar Interrupción
  radio.setPacketReceivedAction(setFlag); 

  // Iniciar escucha asíncrona
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("{\"sistema\": \"Error_Escucha\"}");
    while (true);
  }
}

void loop() {
  if (paqueteRecibido) {
    paqueteRecibido = false; 

    String str;
    int state = radio.readData(str); 

    if (state == RADIOLIB_ERR_NONE) {
      // 1. PARSEO DEL PAYLOAD LORA
      // Formato esperado: NODO_ID,ALERTA,LeakyBucket,GasRes
      int firstComma = str.indexOf(',');
      int secondComma = str.indexOf(',', firstComma + 1);
      int thirdComma = str.indexOf(',', secondComma + 1);

      // 2. FILTRO DE SEGURIDAD (Validar que es un paquete IgnisEdge válido)
      if (firstComma > 0 && secondComma > 0 && thirdComma > 0) {
        String nodo_id = str.substring(0, firstComma);
        String tipo_mensaje = str.substring(firstComma + 1, secondComma);
        String bucket_val = str.substring(secondComma + 1, thirdComma);
        String gas_val = str.substring(thirdComma + 1);

        if (tipo_mensaje == "ALERTA") {
          // 3. SALIDA JSON PARA BACKEND EN FEDORA LINUX
          String jsonPayload = "{\"nodo\":\"" + nodo_id + "\", \"evento\":\"INCENDIO\", \"bucket\":" + bucket_val + ", \"gas_ohms\":" + gas_val + "}";
          Serial.println(jsonPayload); 

          // 4. ACTUALIZACIÓN DE INTERFAZ OLED DE EMERGENCIA
          display.clearDisplay();
          display.setCursor(0,0);
          display.setTextSize(1);
          display.println("!! EMERGENCIA !!");
          display.drawLine(0, 10, 128, 10, WHITE);
          //test
          display.setCursor(0, 20);
          display.print("Origen: "); display.println(nodo_id);
          display.print("Bckt:   "); display.println(bucket_val);
          display.print("Gas:    "); display.print(gas_val); display.println(" Ohm");
          
          // Invertir pantalla para efecto visual de alarma
          display.invertDisplay(true);
          display.display();
          delay(500); // Pequeño parpadeo visual
          display.invertDisplay(false);
          display.display();
        }
      } else {
        // Paquete basura o de otra red LoRa
        Serial.println("{\"evento\": \"Paquete_Desconocido\", \"raw\": \"" + str + "\"}");
      }
    }
// test
    // 5. REACTIVAR ANTENA
    radio.startReceive();
  }
}