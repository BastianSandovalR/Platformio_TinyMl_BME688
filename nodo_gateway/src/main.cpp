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

// Bandera lógica para indicar que llegó un paquete por hardware
volatile bool paqueteRecibido = false; // Se utiliza Volatile para que se refresque el valor asociado en cada iteracion, el bool se guarda en en cpu o en cache,
                                      //en cambio en Volatile se guarda en ram lo que obligamos a que sea temporal, 
                                      // porque asi obligamos a que se lea la memoria ram fisica en cada iteracion del loop


ICACHE_RAM_ATTR void setFlag(void) { //Especificamos que este en la IRAM la cual vive en la SRAM...
  paqueteRecibido = true;            //...la IRAM es una memoria super rapida especificamente para dar instrucciones
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
  display.println("GATEWAY ASINCRONO");
  display.display();

  // Inicializar radio LoRa a 915 MHz
  int state = radio.begin(915.0);
  if (state == RADIOLIB_ERR_NONE) {
    Serial.println("[*] Radio SX1262 inicializada con exito.");
  } else {
    Serial.print("[!] Error inicializando radio, codigo: ");
    Serial.println(state);
    while (true);
  }

  // Configurar la función que se ejecutará cuando DIO1 pase a HIGH (Paquete recibido)
  radio.setPacketReceivedAction(setFlag); // simula un eventlistener, esta atento a como esta el estado de la recepcion del mensaje, si es true o false

  // Iniciar la escucha asíncrona permanente (No bloquea el bucle loop)
  state = radio.startReceive();
  if (state != RADIOLIB_ERR_NONE) {
    Serial.println("[!] Error al iniciar el modo escucha.");
    while (true);
  }
}

void loop() {
  // El loop corre libre. Solo entra aquí si el hardware activa la bandera
  if (paqueteRecibido) {
    paqueteRecibido = false; // Reseteamos la bandera de interrupción

    String str;
    int state = radio.readData(str); // Leemos el buffer de la radio
                                    // Calcula el CRC (control de Redundancia Ciclica) la libreria verifica mediante un algoritmo matematico 
                                    //si los bytes llegaron bien o corruptos


    if (state == RADIOLIB_ERR_NONE) {
      // Enviamos directo a tu script de Python en Fedora
      Serial.println(str); 

      // Actualizamos la interfaz visual de la pantalla OLED
      display.clearDisplay();
      display.setCursor(0,0);
      display.setTextSize(1);
      display.println("! PAQUETE RECIBIDO !");
      display.drawLine(0, 12, 128, 12, WHITE);
      display.setCursor(0, 20);
      display.println(str); 
      display.display();
    }

    // Volvemos a activar la escucha asíncrona para el siguiente paquete
    radio.startReceive();
  }
}
