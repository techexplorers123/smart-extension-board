#define THINGER_SERIAL_DEBUG
#define THINGER_OTA_VERSION "1.0.2"
#define EEPROM_SIZE 10
#define CONFIG_FLAG_ADDR 0
#define RESET_COUNTER_ADDR 1
#define first_run 2
#define relay_pin D5
#include <EEPROM.h>
#include <ThingerESP8266WebConfig.h>
#include <ThingerESP8266OTA.h>
#include <ArduinoJson.h>
ThingerESP8266WebConfig thing;
ThingerESP8266OTA ota(thing);
unsigned long press_window = 1000;
bool in_config_portal = false;
char USERNAME[30];
char password[64];
char DEVICE[100];
String getSanitizedMac()
{
    String mac = WiFi.macAddress();
    mac.replace(":", "");
    return mac;
}

const char *MAC_replace()
{
    static String sanitizedMac = getSanitizedMac();
    return sanitizedMac.c_str();
}

void provision_device(const char *host, uint16_t port, const char *username, const char *passwd, const char *device, const char *name, const char *device_credentials)
{
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    httpsClient.setTimeout(15000); // 15 Seconds
    Serial.print("HTTPS Connecting");
    int r = 0; // retry counter
    while ((!httpsClient.connect(host, port)) && (r < 30))
    {
        delay(100);
        Serial.print(".");
        r++;
    }
    if (r == 30)
    {
        Serial.println("Connection failed");
    }
    else
    {
        Serial.println("Connected to web");
    }
    String getData, Link, access_token;
    String url = "/oauth/token";
    String payload = "grant_type=password&username=" + String(username) + "&password=" + String(passwd);
    int str_len = payload.length();
    httpsClient.print(String("POST ") + url + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Content-Type: application/x-www-form-urlencoded" + "\r\n" + "Content-Length: " + String(str_len) + "\r\n" + "Connection: close\r\n\r\n" + payload);
    String response = "";
    while (httpsClient.connected() || httpsClient.available())
    {
        if (httpsClient.available())
        {
            response += httpsClient.readString();
        }
    }
    Serial.println("Raw response:");
    Serial.println(response);
    int jsonStart = response.indexOf('{');
    if (jsonStart == -1)
    {
        Serial.println("JSON body not found.");
        return;
    }
    String jsonPart = response.substring(jsonStart);

    StaticJsonDocument<1024> doc1;
    DeserializationError error = deserializeJson(doc1, jsonPart);
    if (error)
    {
        Serial.print("JSON Parse Failed: ");
        Serial.println(error.c_str());
        Serial.println(jsonPart);
    }
    else
    {
        access_token = doc1["access_token"].as<String>();
        Serial.println("Access Token: " + access_token);
    }
    StaticJsonDocument<500> doc;
    JsonObject root = doc.to<JsonObject>();
    root["type"] = "Generic";
    root["device"] = device;
    root["name"] = name;
    root["credentials"] = device_credentials;
    char JSONmessageBuffer[200];
    size_t size = serializeJson(root, JSONmessageBuffer);
    String body((const char *)JSONmessageBuffer);
    Serial.print("USERNAME: ");
    Serial.println(USERNAME);
    Link = "/v1/users/" + String(USERNAME) + "/devices?authorization=" + access_token;
    Serial.print("requesting URL: ");
    Serial.println(Link);
    Serial.println(body);
    httpsClient.print(String("POST ") + Link + " HTTP/1.1\r\n" + "Host: " + host + "\r\n" + "Content-Type: application/json" + "\r\n" + "Content-Length: " + String(size) + "\r\n" + "Connection: close\r\n\r\n" + body);
    while (httpsClient.connected())
    {
        if (httpsClient.available())
        {
            char str = httpsClient.read();
            Serial.print(str);
        }
    }
}

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
    thing.add_setup_parameter("password", "password", "", 200);
    thing.add_setup_parameter("name", "device name", "ESP device", 100);
    if (EEPROM.read(CONFIG_FLAG_ADDR) == 1)
    {
        Serial.println(">> Detected config flag! Starting config mode...");
        in_config_portal = true;
        EEPROM.write(CONFIG_FLAG_ADDR, 0);
        EEPROM.write(RESET_COUNTER_ADDR, 0);
        EEPROM.write(first_run, 1);
        EEPROM.commit();
    }
    if (EEPROM.read(first_run) == 1)
    {
        thing.set_on_config_callback([](pson &config)
                                     {
                                        strcpy(USERNAME, config["user"]);
            strcpy(password, config["password"]);
            strcpy(DEVICE, config["name"]);
            provision_device("api.thinger.io", 443, USERNAME, password, "smart_extension", DEVICE, MAC_replace()); });
        EEPROM.write(first_run, 0);
        EEPROM.commit();
    }
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); // LED off initially
    pinMode(relay_pin, OUTPUT);
    if (WiFi.status() == WL_CONNECTED)
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
    thing["switch"] << digitalPin(relay_pin);
    uint8_t resets = EEPROM.read(RESET_COUNTER_ADDR);
    if (resets >= 2)
    {
        startConfigPortal();
    }
    else
    {
        EEPROM.write(RESET_COUNTER_ADDR, resets + 1);
        EEPROM.commit();
    }
    thing.set_device("smart_extension");
    thing.set_credential(MAC_replace());
}

void loop()
{
    unsigned long now = millis();
    if (now > press_window)
    {
        EEPROM.write(RESET_COUNTER_ADDR, 0);
        EEPROM.commit();
    }
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