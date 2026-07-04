#include <Arduino.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "secrets.h" // 機密情報ファイルのインクルード

enum SensorStatus
{
  SENSOR_DISCONNECTED = 0,
  SENSOR_OK = 1
};
struct TmpHumInfo
{
  SensorStatus status; // 0: 未接続, 1: 正常
  float tmp;           // AHT21の温度
  float hum;           // AHT21の湿度 (※BMP280は湿度非対応のためAHT21から取得)
};

// --- ピン配置設定 ---
#define BUTTON_PIN 3 // 復帰・更新用物理ボタン (XIAO D3)

// --- OLED 設定 (128x64) ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- タイマー・ステート管理 ---
unsigned long lastActivityTime = 0;
const unsigned long ONDEMAND_TIMEOUT = 60000; // 60秒で消灯
bool isDisplayOn = true;

// --- WiFi 固定IP ＆ TCP設定 ---
const char *ssid = SECRET_SSID;
const char *password = SECRET_PASSWORD;
const int port = 5000;
WiFiServer server(port);

// secrets.h のマクロから IPAddress を生成
IPAddress local_IP(SECRET_LOCAL_IP);
IPAddress gateway(SECRET_GATEWAY);
IPAddress subnet(SECRET_SUBNET);
IPAddress primaryDNS(SECRET_DNS_1);
IPAddress secondaryDNS(SECRET_DNS_2);

// --- I2Cマルチプレクサ ＆ センサー設定 ---
#define TCA9548A_ADDR 0x70
#define NUM_SENSORS 6
Adafruit_AHTX0 aht;
Adafruit_BMP280 bmp;

TmpHumInfo sensorData[NUM_SENSORS];

// TCA9548Aのチャンネル切り替え
void selectI2CChannel(uint8_t channel)
{
  if (channel > 7)
    return;
  Wire.beginTransmission(TCA9548A_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

// I2Cバス上のデバイス死活監視用チェック
bool checkI2CDeviceConnected(uint8_t address)
{
  Wire.beginTransmission(address);
  return (Wire.endTransmission() == 0);
}

// OLED表示更新用関数 - システムメッセージ表示
void updateOLEDDisplay(String msg)
{
  if (!isDisplayOn)
    return;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.println("=== SYSTEM STATUS ===");
  display.printf("WiFi: %s\n", (WiFi.status() == WL_CONNECTED) ? "CONNECTED" : "DISCONNECTED");
  display.print("IP: ");
  display.println(WiFi.localIP().toString());
  display.println("---------------------");
  display.println(msg);
  display.display();
}

// 湿度表示用関数
void displayHumidityData()
{
  if (!isDisplayOn)
    return;

  display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.println("=== HUMIDITY ===");

  // 6個のセンサーの湿度を表示 (2列表示)
  for (uint8_t i = 0; i < NUM_SENSORS; i += 2)
  {
    display.printf("S%d:", i + 1);
    if (sensorData[i].status == SENSOR_OK)
    {
      display.printf("%.1f%%  ", sensorData[i].hum);
    }
    else
    {
      display.print("N/A  ");
    }

    if (i + 1 < NUM_SENSORS)
    {
      display.printf("S%d:", i + 2);
      if (sensorData[i + 1].status == SENSOR_OK)
      {
        display.printf("%.1f%%\n", sensorData[i + 1].hum);
      }
      else
      {
        display.print("N/A\n");
      }
    }
  }

  display.display();
}

// センサーデータ読み込み関数
void readAllSensors()
{
  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    selectI2CChannel(i);
    delay(5);

    bool aht_online = checkI2CDeviceConnected(0x38);

    if (aht_online && aht.begin())
    {
      sensors_event_t humidity, temp;
      aht.getEvent(&humidity, &temp);

      sensorData[i].status = SENSOR_OK;
      sensorData[i].tmp = temp.temperature;
      sensorData[i].hum = humidity.relative_humidity;
    }
    else
    {
      sensorData[i].status = SENSOR_DISCONNECTED;
      sensorData[i].tmp = NAN;
      sensorData[i].hum = NAN;
    }
  }
}

// 画面点灯
void turnOnDisplay()
{
  if (!isDisplayOn)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    isDisplayOn = true;
    Serial.println("OLED Display: ON");
  }
  lastActivityTime = millis(); // 消灯タイマーリセット
}

// 画面消灯 (省電力スリープ)
void turnOffDisplay()
{
  if (isDisplayOn)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF);
    isDisplayOn = false;
    Serial.println("OLED Display: OFF (Timeout)");
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // I2C初期化 (SDA:GPIO6, SCL:GPIO7)
  Wire.begin(6, 7);

  // OLED初期化
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR))
  {
    Serial.println(F("SSD1306 initialization failed"));
    for (;;)
      ;
  }
  display.clearDisplay();
  display.display();

  turnOnDisplay();
  updateOLEDDisplay("Booting...\nConfiguring IP...");

  // 固定IPの適用
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Configuration Failed");
  }

  // WiFi接続
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  server.begin();
  Serial.println("\nTCP Server Started.");

  // 起動時のセンサー初期化 (未接続 ch があってもフリーズさせない)
  for (uint8_t i = 0; i < NUM_SENSORS; i++)
  {
    selectI2CChannel(i);
    delay(5);
    if (checkI2CDeviceConnected(0x38))
      aht.begin();
    if (checkI2CDeviceConnected(0x76))
      if (bmp.begin(0x77))
      {
        bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                        Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                        Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                        Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                        Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
      }
  }

  updateOLEDDisplay("System Ready.\nWaiting for client...");
}

void loop()
{
  // 1. 物理ボタン入力の監視 (画面復帰・センサー読み込み・表示)
  if (digitalRead(BUTTON_PIN) == LOW)
  {
    Serial.println("Button Triggered!");
    turnOnDisplay();
    readAllSensors();
    displayHumidityData();
    delay(300); // チャタリング防止用ディレイ
  }

  // 2. 画面タイムアウト消灯の監視
  if (isDisplayOn && (millis() - lastActivityTime > ONDEMAND_TIMEOUT))
  {
    turnOffDisplay();
  }

  // 3. TCPサーバー処理 (画面制御へは一切干渉せず、バックグラウンドで処理)
  WiFiClient client = server.available();
  if (client)
  {
    String request = "";
    while (client.connected())
    {
      if (client.available())
      {
        char c = client.read();
        request += c;

        if (c == '\n')
        {
          request.trim();

          if (request == "GET_DATA")
          {
            String response = "{\n  \"sensors\": [\n";

            for (uint8_t i = 0; i < NUM_SENSORS; i++)
            {
              // JSONデータの組み立て (キャッシュされたセンサーデータを使用)
              response += String("    {\"ch\":") + i +
                          ", \"status\":" + (sensorData[i].status == SENSOR_OK ? 1 : 0) +
                          ", \"aht_temp\":" + sensorData[i].tmp +
                          ", \"humidity\":" + sensorData[i].hum + "}";

              if (i < NUM_SENSORS - 1)
                response += ",\n";
            }
            response += "\n  ]\n}\n";
            client.print(response);
            Serial.println("Sent response to client:\n" + response);
          }
          else
          {
            client.println("ERROR");
          }
          break;
        }
      }
    }
    delay(10);
    client.stop();
  }
}
