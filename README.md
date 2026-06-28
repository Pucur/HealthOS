# 🥗 HealthOS (VERY BETA)

![ESP32](https://img.shields.io/badge/ESP32-IoT-blue)
![FastAPI](https://img.shields.io/badge/FastAPI-Backend-green)
![LVGL](https://img.shields.io/badge/LVGL-UI-orange)
![Home Assistant](https://img.shields.io/badge/HomeAssistant-Integration-purple)

HealthOS is a local IoT calorie tracking system built on ESP32 with a touchscreen UI, FastAPI backend, and Home Assistant integration.

---

## 🚀 Features

- 🍔 Touch-based food logging  
- 📊 Daily calorie tracking  
- ⚡ Real-time WiFi communication  
- 🌈 Color-coded calorie status  
- 📡 REST API backend (FastAPI + SQLite)  
- 🏠 Home Assistant integration  
- 🔆 Automatic display brightness control  
- 📱 LVGL touchscreen UI  
- 🔄 Auto-refreshing food database  

---

## 🖥️ ESP32 Side

- ESP32 (Arduino framework)  
- LVGL UI library  
- Arduino_GFX  
- AXS5106L touch controller  
- WiFi + HTTPClient  
- PWM backlight control  

---

## 🐍 Backend

- FastAPI (Python)  
- SQLite database  
- Pydantic models  

---

## 🏠 Integration

- Home Assistant REST sensors  
```
sensor:
  - platform: rest
    name: "HealthOS Calorie"
    resource: http://SERVER_IP:8000/today
    value_template: "{{ value_json.total_kcal | float | round(0) | int }}"
    unit_of_measurement: "kcal"
    scan_interval: 60
```

---

## 🌡️ Calorie Logic

| Range (kcal) | Status        | Color   |
|--------------|--------------|--------|
| ≤ 2000       | Normal       | 🟢 Green |
| 2000–2900    | Warning      | 🟡 Yellow |
| > 2900       | High intake  | 🔴 Red |

---

## 🔄 Data Flow

1. ESP32 fetches `/foods`
2. UI generates buttons dynamically
3. User selects food
4. ESP32 sends `POST /log`
5. Backend updates `/today`
6. ESP32 + Home Assistant refresh data

---

## 📡 API Endpoints

### GET /foods
Returns available food items.

### POST /log
Logs a food entry.

### GET /today
Returns daily calorie summary.

---

## 📱 UI Behavior

- Scrollable food list  
- Long-press protection against accidental input  
- Auto refresh every 30 seconds  
- Screen dimming on inactivity  

---

## 🔮 Future Improvements

- Multi-user support  
- Weekly / monthly analytics  
- Cloud sync  
- QR / barcode scanning  
- Macro tracking (protein / fat / carbs)  
- Graph dashboard UI  

---

## TESTED WITH

- Waveshare ESP32-C6-Touch-LCD-1.47 (I will replace to ESP32-S3 3.5" touch because thats bigger)
- Raspberry Pi 5 (server)

## ⚠️ Notes

- Local network only  
- No external cloud dependency  
- Runs on `192.168.x.x` private IP range  

---

## 📜 License

MIT
