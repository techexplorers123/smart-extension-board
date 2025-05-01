#define THINGER_SERIAL_DEBUG
#define THINGER_OTA_VERSION "1.0.2"
#define EEPROM_SIZE 10
#define CONFIG_FLAG_ADDR 0
#define RESET_COUNTER_ADDR 1
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

void provision_device(const char *host, uint16_t port, const char *username, const char *passwd, const char *name, const char *device_credentials)
{
    WiFiClientSecure httpsClient;
    httpsClient.setInsecure();
    httpsClient.setTimeout(15000);
    String authPayload = String("grant_type=password&username=") + username + "&password=" + passwd;
    Serial.print("Connecting for token…");
    httpsClient.connect(host, port);
    httpsClient.printf(
        "POST /oauth/token HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Content-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        host,
        authPayload.length(),
        authPayload.c_str());
    String resp = "";
    while (httpsClient.connected() || httpsClient.available())
    {
        if (httpsClient.available())
        {
            resp += httpsClient.readString();
        }
    }
    httpsClient.stop();
    int idx = resp.indexOf("{");
    if (idx < 0)
    {
        Serial.println("No HTTP body in token response");
        return;
    }
    String body = resp.substring(idx);
    StaticJsonDocument<512> json;
    auto err = deserializeJson(json, body);
    if (err)
    {
        Serial.print("Token JSON parse error: ");
        Serial.println(err.c_str());
        Serial.println(body);
        return;
    }
    String access_token = json["access_token"].as<String>();
    Serial.println("Got access token: " + access_token);
    StaticJsonDocument<256> doc;
    doc["type"] = "Generic";
    doc["device"] = "smart_extension";
    doc["name"] = name;
    doc["credentials"] = device_credentials;
    String deviceBody;
    serializeJson(doc, deviceBody);
    WiFiClientSecure postClient;
    postClient.setInsecure();
    postClient.setTimeout(15000);
    Serial.print("Connecting to create device…");
    postClient.connect(host, port);
    postClient.printf(
        "POST /v1/users/%s/devices HTTP/1.1\r\n"
        "Host: %s\r\n"
        "Authorization: Bearer %s\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n\r\n"
        "%s",
        username,
        host,
        access_token.c_str(),
        deviceBody.length(),
        deviceBody.c_str());
    String resp2 = "";
    while (postClient.connected() || postClient.available())
    {
        if (postClient.available())
        {
            resp2 += char(postClient.read());
        }
    }
    postClient.stop();
    Serial.println("Device creation response:");
    Serial.println(resp2);
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
        EEPROM.commit();
    }
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);
    pinMode(relay_pin, OUTPUT);
    if (WiFi.status() == WL_CONNECTED)
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
    thing["switch"] << digitalPin(relay_pin);
    uint8_t resets = EEPROM.read(RESET_COUNTER_ADDR);
    if (resets > 2)
    {
        startConfigPortal();
    }
    else
    {
        EEPROM.write(RESET_COUNTER_ADDR, resets + 1);
        EEPROM.commit();
    }
    thing.set_device("smart_extension");
    thing.set_credential("smart_extension123");
    WiFi.mode(WIFI_STA);
    WiFi.begin();
    Serial.print("Joining WiFi");
    uint8_t retries = 0;
    while (WiFi.status() != WL_CONNECTED && retries++ < 20)
    {
        Serial.print(".");
    }
    Serial.println(" OK");
    thing.set_on_config_callback([](pson &config)
                                 {
strcpy(USERNAME, config["user"]);
strcpy(password, config["password"]);
strcpy(DEVICE, config["name"]);
provision_device("api.thinger.io", 443, USERNAME, password, DEVICE, "smart_extension123"); });
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