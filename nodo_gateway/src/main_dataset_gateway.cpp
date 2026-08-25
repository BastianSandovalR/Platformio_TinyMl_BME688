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
  display.println("GATEWAY LISTO");
  display.display();

  radio.begin(915.0);
}

void loop() {
  String str;
  if (radio.receive(str) == RADIOLIB_ERR_NONE) {
    Serial.println(str); 

    display.clearDisplay();
    display.setCursor(0,0);
    display.setTextSize(1);
    display.println("! PAQUETE RECIBIDO !");
    display.setCursor(0, 15);
    display.println(str); 
    display.display();
  } 
}