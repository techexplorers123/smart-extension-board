#define THINGER_SERIAL_DEBUG
#define THINGER_OTA_VERSION "1.0.0"
#define EEPROM_SIZE 1
#define CONFIG_FLAG_ADDR 0
#define relay_pin D5
#define BUTTON_PIN 0
#include <EEPROM.h>
#include <ThingerESP8266WebConfig.h>
#include <ThingerESP8266OTA.h>
ThingerESP8266WebConfig thing;
ThingerESP8266OTA ota(thing);
unsigned long last_press_time = 0;
unsigned long press_window = 3000;
int press_count = 0;
bool button_last_state = HIGH;
bool in_config_portal = false;
void startConfigPortal()
{
  Serial.println(">> Starting Configuration Portal...");
  thing.clean_credentials();
  EEPROM.write(CONFIG_FLAG_ADDR, 1);
  EEPROM.commit();
  delay(100);
  thing.reboot();
}

void setup()
{
  Serial.begin(115200);
  EEPROM.begin(EEPROM_SIZE);
  if (EEPROM.read(CONFIG_FLAG_ADDR) == 1)
  {
    Serial.println(">> Detected config flag! Starting config mode...");
    EEPROM.write(CONFIG_FLAG_ADDR, 0); // Clear it after use
    EEPROM.commit();
    in_config_portal = true;
  }
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // LED off initially
  pinMode(relay_pin, OUTPUT);
  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(LED_BUILTIN, LOW);
  }
  thing["switch"] << digitalPin(relay_pin);
}

void loop()
{
  bool button_pressed = digitalRead(BUTTON_PIN) == LOW;
  if (button_pressed && button_last_state == HIGH)
  {
    // Button was just pressed
    unsigned long now = millis();
    if (now - last_press_time > press_window)
    {
      // Reset count if too slow
      press_count = 0;
    }
    press_count++;
    last_press_time = now;
    Serial.printf(">> Button Pressed: %d\n", press_count);

    if (press_count >= 3)
    {
      startConfigPortal();
    }
  }
  button_last_state = !button_pressed ? HIGH : LOW;
  static unsigned long last_blink = 0;
  static bool led_state = false;
  if (in_config_portal)
  {
    if (millis() - last_blink >= 200)
    {
      led_state = !led_state;
      digitalWrite(LED_BUILTIN, led_state ? LOW : HIGH);
      last_blink = millis();
    }
  }
  thing.handle();
}