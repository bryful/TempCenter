# TempCenter

複数の温度・湿度センサーを接続し、TCP経由でデータを提供するESP32デバイス。
表示アプリ [TempCenter_disp](https://github.com/bryful/TempCenter_disp) と対として使用します。

## 主な機能

- TCA9548A I2Cマルチプレクサ経由で最大6チャンネルのセンサーを接続
- AHT21（温度・湿度）および BMP280（温度・気圧）に対応
- SSD1306 OLEDディスプレイによるステータス表示（128x64）
- WiFi固定IP接続 ＆ TCPサーバー（ポート5000）でデータ配信
- `GET_DATA` コマンドに対してJSON形式でセンサーデータを返す
- 物理ボタン（D3）でのオンデマンド表示更新
- 60秒無操作でOLEDを自動消灯（省電力）

## TCPプロトコル

TCPポート `5000` に接続し、以下のコマンドを送信します。

| コマンド   | 説明                             |
| ---------- | -------------------------------- |
| `GET_DATA` | 全センサーデータをJSON形式で返す |

### レスポンス例

```json
{
  "sensors": [
    {"ch":0, "status":1, "aht_temp":25.3, "humidity":55.2},
    {"ch":1, "status":0, "aht_temp":nan, "humidity":nan}
  ]
}
```

- `status`: `1` = 正常、`0` = 未接続

## ハードウェア構成

| 部品               | 説明                                 |
| ------------------ | ------------------------------------ |
| Seeed XIAO ESP32C3 | メインマイコン                       |
| TCA9548A           | I2Cマルチプレクサ（アドレス: 0x70）  |
| AHT21              | 温度・湿度センサー（アドレス: 0x38） |
| BMP280             | 温度・気圧センサー（アドレス: 0x77） |
| SSD1306            | OLEDディスプレイ（アドレス: 0x3C）   |
| ボタン             | GPIO D3（INPUT_PULLUP）              |

- I2C: SDA = GPIO6、SCL = GPIO7

## 対応環境

- [PlatformIO](https://platformio.org/)
- Framework: Arduino
- ボード: Seeed XIAO ESP32C3

## 依存ライブラリ

| ライブラリ              | バージョン |
| ----------------------- | ---------- |
| Adafruit BusIO          | ^1.16.0    |
| Adafruit GFX Library    | ^1.11.11   |
| Adafruit SSD1306        | ^2.5.13    |
| Adafruit AHTX0          | ^2.0.5     |
| Adafruit BMP280 Library | ^2.6.8     |

## セットアップ

1. `include/secrets.h.example` をコピーして `include/secrets.h` を作成します。

```sh
copy include\secrets.h.example include\secrets.h
```

2. `secrets.h` を編集し、WiFi情報と固定IPを設定します。

```c
#define SECRET_SSID       "YOUR_WIFI_SSID"
#define SECRET_PASSWORD   "YOUR_WIFI_PASSWORD"
#define SECRET_LOCAL_IP   192, 168, 1, 150
#define SECRET_GATEWAY    192, 168, 1, 1
#define SECRET_SUBNET     255, 255, 255, 0
#define SECRET_DNS_1      192, 168, 1, 1
#define SECRET_DNS_2      8, 8, 8, 8
```

3. PlatformIO でビルド・書き込みを行います。

```sh
pio run --target upload
```

## ディレクトリ構成

```
TempCenter/
├── src/
│   └── main.cpp          # メインプログラム
├── include/
│   ├── secrets.h         # WiFi・IP設定（git管理外）
│   └── secrets.h.example # secrets.h のテンプレート
├── lib/                  # カスタムライブラリ（未使用時は空）
├── test/                 # テストコード
└── platformio.ini        # PlatformIO設定ファイル
```

