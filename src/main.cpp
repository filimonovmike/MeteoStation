#include <Arduino.h>
#include <U8g2lib.h>
#include <PubSubClient.h>

#include "WiFiConnector.h"
#include "DHT22Sensor.h"
#include "MQTTClient.h"

#define DEBUG

const uint8_t LED = 2;
const int pinDHT22 = 5;

DHT22Sensor dht22(pinDHT22);
WiFiConnector wifi;

MQTTClient mqttClient(wifi);
char msg[50];

// display
U8G2_SSD1306_128X32_UNIVISION_1_HW_I2C u8g2(U8G2_R0, /* reset=*/U8X8_PIN_NONE, /* clock=*/SCK, /* data=*/SDA); // pin remapping with ESP8266 HW I2C
const uint16_t FRAME_DURATION = 1000;
const char wifiConnectedChar = (char)72; // значок wifi для u8g2_font_open_iconic_www_1x_t
const char wifiDisconnectedChar = (char)74;
const char mqttChar = (char)83;
const char DisplayEmptyStatusChar = (char)32; // символ пробела
const char humidityChar = (char)152;

// sensor data
float temperature;
float humidity;

struct DisplayConnectionChar
{
  char current;
  char connected;
  char disconnected;
  uint16_t connection_blink_duration_ms; // длительность мигания иконки при подключении
  uint32_t last_change_ms;               // время последнего изменения иконки статуса
  char getConnectingChar()
  {
    if ((uint32_t)(millis() - last_change_ms) > connection_blink_duration_ms)
    {
      return current == connected ? DisplayEmptyStatusChar : connected;
    }
    return current;
  }
  void setCurrentChar(char c)
  {
    if (current == c)
      return;
    current = c;
    last_change_ms = millis();
  };
};

DisplayConnectionChar wifiDisplayChar = {DisplayEmptyStatusChar, wifiConnectedChar, wifiDisconnectedChar, 700, 0};
DisplayConnectionChar mqttDisplayChar = {DisplayEmptyStatusChar, mqttChar, DisplayEmptyStatusChar, 700, 0};

char getWifiStatusChar()
{
  switch (wifi.getStatus())
  {
  case WF_CONNECTED:
    return wifiDisplayChar.connected;
  case WF_CONNECTING:
    return wifiDisplayChar.getConnectingChar();
  case WF_DISCONNECTED:
    return wifiDisplayChar.disconnected;
  default:
    return wifiDisplayChar.current;
  }
}

char getMqttStatusChar()
{
  switch (mqttClient.getStatus())
  {
  case WF_CONNECTED:
    return mqttDisplayChar.connected;
  case WF_CONNECTING:
    return mqttDisplayChar.getConnectingChar();
  case WF_DISCONNECTED:
    return mqttDisplayChar.disconnected;
  default:
    return mqttDisplayChar.current;
  }
}

void draw(bool force)
{
  bool wifiIconChanged = wifiDisplayChar.current != getWifiStatusChar();
  bool mqttIconChanged = mqttDisplayChar.current != getMqttStatusChar();
  bool needRedraw = force ? force : (wifiIconChanged || mqttIconChanged);
  if (!needRedraw)
    return;

  if (wifiIconChanged)
  {
    wifiDisplayChar.setCurrentChar(getWifiStatusChar());
  }
  if (mqttIconChanged)
  {
    mqttDisplayChar.setCurrentChar(getMqttStatusChar());
  }

  // Отправляем данные на дисплей
  u8g2.firstPage();
  do
  {
    // wifi icon
    u8g2.setFont(u8g2_font_open_iconic_www_1x_t);
    u8g2.setCursor(120, 8);
    u8g2.print(wifiDisplayChar.current);
    // mqtt
    u8g2.setCursor(108, 8);
    u8g2.print(mqttDisplayChar.current);

    // Humudity
    if (humidity)
    {
      u8g2.setFont(u8g2_font_open_iconic_all_2x_t);
      u8g2.setCursor(70, 32);
      u8g2.print(humidityChar);
      char t[10];
      sprintf(t, "%d%% ", (int)humidity);
      u8g2.setFont(u8g2_font_9x18_tf);
      u8g2.setCursor(90, 32);
      u8g2.print(t);
    }

    // Temperature
    if (temperature)
    {
      char t[10];
      dtostrf(temperature, 5, 1, t);
      sprintf(t, "%s\u00b0C", t);
      u8g2.setFont(u8g2_font_10x20_tf);
      u8g2.setCursor(0, 14);
      u8g2.print(t);
    }

  } while (u8g2.nextPage());
#ifdef DEBUG
  Serial.println(F("Draw!"));
#endif
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  // pinMode(LED, OUTPUT);
  // digitalWrite(LED, HIGH);
  wifi.init();
  wifi.setup();

  u8g2.begin();
  u8g2.enableUTF8Print();
  u8g2.setFontDirection(0);
}

void loop()
{
  // put your main code here, to run repeatedly:
  wifi.tick();
  mqttClient.loop();
  dht22.getDataFromSensor();
  bool temperatureChanged = temperature != dht22.getTemperature();
  bool humidityChanged = humidity != dht22.getHumidity();
  uint32_t current_ms = millis();
  if (temperatureChanged)
  {
    temperature = dht22.getTemperature();

    char msg[20];
    dtostrf(temperature, 5, 1, msg);
    mqttClient.publishTemperature(msg);
    // last_send_mqtt_data_ms = current_ms;
  }
  if (humidityChanged)
  {
    humidity = dht22.getHumidity();
    char msg[20];
    sprintf(msg, "%d", (int)humidity);
    mqttClient.publishHumidity(msg);
    // last_send_mqtt_data_ms = current_ms;
  }
  draw(temperatureChanged || humidityChanged);
}