# 🩺 Smart MediBox  

A smart **IoT-based medicine reminder and monitoring system** powered by **ESP32**.  
This project combines **real-time alarms, environment monitoring, and MQTT connectivity** to ensure timely medicine intake and safe storage conditions.  

---

## ✨ Features  
- ⏰ **Medicine Reminder Alarms** with snooze and cancel options  
- 🌡️ **Temperature & Humidity Monitoring** using DHT22  
- 💡 **Light Intensity Tracking** with LDR sensor  
- 🔊 **Buzzer & LED Alerts** for medicine time and abnormal conditions  
- 🕒 **NTP-based Real-Time Clock** (auto-sync with internet)  
- 🔄 **MQTT Communication** for remote monitoring and configuration  
- ⚙️ **Servo Motor Control** for automated pillbox/compartment movement  
- 📟 **OLED Display Interface** with menu navigation buttons  

---

## 🛠️ Hardware Used  
- **ESP32** Development Board  
- **OLED Display (SSD1306, I2C)**  
- **DHT22 Temperature & Humidity Sensor**  
- **LDR Sensor**  
- **Servo Motor (SG90 or compatible)**  
- **Buzzer**  
- **LED Indicators**  
- **Push Buttons** (for menu navigation and alarm control)  

---

## 📡 Software & Libraries  
This project uses the following Arduino libraries:  
- `Wire.h`  
- `Adafruit_GFX.h`  
- `Adafruit_SSD1306.h`  
- `DHT.h`  
- `WiFi.h`  
- `NTPClient.h`  
- `PubSubClient.h`  
- `ESP32Servo.h`  

---

