#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <WiFi.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ESP32Servo.h>

// OLED Display Configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// DHT sensor setup
#define DHTPIN 4
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// Buzzer and LED setup
#define BUZZER_PIN 5
#define LED_PIN 2
#define LED_PIN_TH 17

// Button setup
#define BUTTON_UP 12
#define BUTTON_DOWN 14
#define BUTTON_OK 27
#define BUTTON_CANCEL 26
#define BUTTON_SNOOZE 25

// NTP client setup
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

// MQTT Configuration
//test.mosquitto.org
//broker.hivemq.com
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 1883;
WiFiClient espClient;
PubSubClient client(espClient);

// MQTT Topics
const char* light_intensity_topic = "medibox/light_intensity";
const char* temperature_topic = "medibox/temperature";
const char* motor_angle_topic = "medibox/motor_angle";
const char* config_topic = "medibox/config";
const char* status_topic = "medibox/status";

// Sensor pins
#define LDR_PIN 34
#define SERVO_PIN 13
Servo servo;

// Alarm variables
int timeZoneOffset = 0;
int alarmHours[2] = {-1, -1};
int alarmMinutes[2] = {-1, -1};
int alarmSeconds[2] = {0, 0};
bool alarmEnabled[2] = {false, false};
bool alarmTriggered[2] = {false, false};
int currentMode = 0;
const char* menuOptions[] = {"SET_Time_Zone", "SET_Alarm 1", "SET_Alarm 2", "View_Alarms", "Delete_Alarm", "System_Config"};
const int maxModes = 6;
bool menuActive = false;
unsigned long snoozeTime = 0;
const unsigned long snoozeDuration = 5 * 60 * 1000;

// Light monitoring variables - renamed gamma to gamma_val to avoid conflict
float sampling_interval = 5.0;
float sending_interval = 120.0;
float theta_offset = 30.0;
float gamma_val = 0.75;  // Changed from gamma to gamma_val
float T_med = 30.0;
float light_readings[24];
int reading_index = 0;
bool initialized = false;
unsigned long last_sample_time = 0;
unsigned long last_send_time = 0;

void setup() {
  Serial.begin(115200);

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.display();
  delay(2000);
  display.clearDisplay();

  // Initialize DHT sensor
  dht.begin();

  // Initialize buttons
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_OK, INPUT_PULLUP);
  pinMode(BUTTON_CANCEL, INPUT_PULLUP);
  pinMode(BUTTON_SNOOZE, INPUT_PULLUP);

  // Initialize buzzer and LEDs
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(LED_PIN, OUTPUT);
  pinMode(LED_PIN_TH, OUTPUT);

  // Initialize servo with ESP32Servo
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  servo.setPeriodHertz(50);    // Standard 50hz servo
  servo.attach(SERVO_PIN, 500, 2500);
  servo.write(theta_offset);

  // Initialize light readings array
  for (int i = 0; i < 24; i++) {
    light_readings[i] = 0;
  }

  // Connect to WiFi
  WiFi.begin("Wokwi-GUEST", "");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to Wi-Fi");

  // Initialize NTP client
  timeClient.begin();
  timeClient.setTimeOffset(timeZoneOffset * 60);

  // Initialize MQTT
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  // Display welcome message
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Medibox System");
  display.display();
  delay(2000);
  display.clearDisplay();
}
void loop() {
  // Update time and check alarms
  timeClient.update();
  updateTimeWithCheckAlarm();

  // Handle menu
  if (digitalRead(BUTTON_CANCEL) == LOW) {
    delay(200);
    menuActive = !menuActive;
    while (digitalRead(BUTTON_CANCEL) == LOW);
  }

  if (menuActive) {
    goToMenu();
  } else {
    // Main operation mode
    checkTempAndHumidity();
    handleLightMonitoring();
  }

  // Handle MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  delay(100);
}

void printLine(String message, int textSize, int row, int column) {
  display.setTextSize(textSize);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(column, row);
  display.println(message);
  display.display();
}

void updateTime() {
  timeClient.update();
}

void printTimeNow() {
  String formattedTime = timeClient.getFormattedTime();
  printLine(formattedTime, 2, 51, 0);
}

void updateTimeWithCheckAlarm() {
  updateTime();
  printTimeNow();

  for (int i = 0; i < 2; i++) {
    if (alarmEnabled[i] && !alarmTriggered[i]) {
      int currentHour = timeClient.getHours();
      int currentMinute = timeClient.getMinutes();
      int currentSecond = timeClient.getSeconds();

      if (currentHour == alarmHours[i] && currentMinute == alarmMinutes[i] && currentSecond == alarmSeconds[i]) {
        ringAlarm();
        alarmTriggered[i] = true;
      }
    }
  }
}

void ringAlarm() {
  Serial.println("Ringing alarm...");
  digitalWrite(LED_PIN, HIGH);
  tone(BUZZER_PIN, 2000);
  printLine("Medicine Time!", 1, 30, 0);

  while (digitalRead(BUTTON_OK) == HIGH && digitalRead(BUTTON_SNOOZE) == HIGH) {
    // Wait for button press
  }

  if (digitalRead(BUTTON_SNOOZE) == LOW) {
    snoozeAlarm();
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(LED_PIN, LOW);
  }
}

void snoozeAlarm() {
  snoozeTime = millis();
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);
  
  display.clearDisplay();
  printLine("Snoozed->5min", 1, 40, 0);
  display.display();
  delay(2000);

  while (millis() - snoozeTime < snoozeDuration) {
    display.clearDisplay();
    printLine("Snoozed->5min", 1, 0, 0);
    updateTime();
    printTimeNow();
    display.display();
    delay(500);
  }

  ringAlarm();
}

int waitForButtonPress() {
  while (true) {
    if (digitalRead(BUTTON_UP) == LOW) {
      delay(200);
      return BUTTON_UP;
    }
    if (digitalRead(BUTTON_DOWN) == LOW) {
      delay(200);
      return BUTTON_DOWN;
    }
    if (digitalRead(BUTTON_OK) == LOW) {
      delay(200);
      return BUTTON_OK;
    }
    if (digitalRead(BUTTON_CANCEL) == LOW) {
      delay(200);
      return BUTTON_CANCEL;
    }
    updateTime();
  }
}

void goToMenu() {
  display.clearDisplay();
  printLine(menuOptions[currentMode], 1, 0, 0);
  printTimeNow();

  if (strcmp(menuOptions[currentMode], "View_Alarms") == 0) {
    for (int i = 0; i < 2; i++) {
      if (alarmEnabled[i]) {
        printLine("Alarm " + String(i + 1) + ": " + String(alarmHours[i]) + ":" + String(alarmMinutes[i]) + ":" + String(alarmSeconds[i]), 1, (i + 2) * 10, 0);
      } else {
        printLine("Alarm " + String(i + 1) + ": Disabled", 1, (i + 2) * 10, 0);
      }
    }
  }

  int pressed = waitForButtonPress();
  if (pressed == BUTTON_UP) {
    currentMode = (currentMode + 1) % maxModes;
  } else if (pressed == BUTTON_DOWN) {
    currentMode = (currentMode - 1 + maxModes) % maxModes;
  } else if (pressed == BUTTON_OK) {
    runMode(currentMode);
    menuActive = false;
  } else if (pressed == BUTTON_CANCEL) {
    menuActive = false;
  }
}

void runMode(int mode) {
  switch (mode) {
    case 0: setTimeZone(); break;
    case 1: setAlarm(0); break;
    case 2: setAlarm(1); break;
    case 3: viewAlarms(); break;
    case 4: deleteAlarm(); break;
  }
}

void setTimeZone() {
  int tempHours = timeZoneOffset / 60;
  int tempMinutes = timeZoneOffset % 60;

  while (true) {
    display.clearDisplay();
    printLine("Set_Time_Zone_Hours: " + String(tempHours), 1, 0, 0);

    int pressed = waitForButtonPress();
    if (pressed == BUTTON_UP) {
      tempHours = (tempHours + 1) % 24;
    } else if (pressed == BUTTON_DOWN) {
      tempHours = (tempHours - 1 + 24) % 24;
    } else if (pressed == BUTTON_OK) {
      break;
    } else if (pressed == BUTTON_CANCEL) {
      return;
    }
  }

  while (true) {
    display.clearDisplay();
    printLine("Set_Time_Zone_Minutes: " + String(tempMinutes), 1, 0, 0);

    int pressed = waitForButtonPress();
    if (pressed == BUTTON_UP) {
      tempMinutes = (tempMinutes + 1) % 60;
    } else if (pressed == BUTTON_DOWN) {
      tempMinutes = (tempMinutes - 1 + 60) % 60;
    } else if (pressed == BUTTON_OK) {
      break;
    } else if (pressed == BUTTON_CANCEL) {
      return;
    }
  }

  timeZoneOffset = (tempHours * 60) + tempMinutes;
  timeClient.setTimeOffset(timeZoneOffset * 60);

  display.clearDisplay();
  printLine("Time Zone Set", 2, 0, 0);
  delay(1000);
}

void setAlarm(int alarm) {
  int tempHour = alarmHours[alarm];
  int tempMinute = alarmMinutes[alarm];
  int tempSecond = 0;

  while (true) {
    display.clearDisplay();
    printLine("Set_Alarm " + String(alarm + 1) + " Hour: " + String(tempHour), 2, 0, 0);

    int pressed = waitForButtonPress();
    if (pressed == BUTTON_UP) {
      tempHour = (tempHour + 1) % 24;
    } else if (pressed == BUTTON_DOWN) {
      tempHour = (tempHour - 1 + 24) % 24;
    } else if (pressed == BUTTON_OK) {
      alarmHours[alarm] = tempHour;
      break;
    } else if (pressed == BUTTON_CANCEL) {
      return;
    }
  }

  while (true) {
    display.clearDisplay();
    printLine("Set_Alarm " + String(alarm + 1) + " Minute: " + String(tempMinute), 2, 0, 0);

    int pressed = waitForButtonPress();
    if (pressed == BUTTON_UP) {
      tempMinute = (tempMinute + 1) % 60;
    } else if (pressed == BUTTON_DOWN) {
      tempMinute = (tempMinute - 1 + 60) % 60;
    } else if (pressed == BUTTON_OK) {
      alarmMinutes[alarm] = tempMinute;
      break;
    } else if (pressed == BUTTON_CANCEL) {
      return;
    }
  }

  while (true) {
    display.clearDisplay();
    printLine("Set_Alarm " + String(alarm + 1) + " Second: " + String(tempSecond), 2, 0, 0);

    int pressed = waitForButtonPress();
    if (pressed == BUTTON_UP) {
      tempSecond = (tempSecond + 1) % 60;
    } else if (pressed == BUTTON_DOWN) {
      tempSecond = (tempSecond - 1 + 60) % 60;
    } else if (pressed == BUTTON_OK) {
      alarmSeconds[alarm] = tempSecond;
      alarmEnabled[alarm] = true;
      break;
    } else if (pressed == BUTTON_CANCEL) {
      return;
    }
  }

  display.clearDisplay();
  printLine("Alarm is set", 2, 0, 0);
  delay(1000);
}

void viewAlarms() {
  display.clearDisplay();
  for (int i = 0; i < 2; i++) {
    if (alarmEnabled[i]) {
      printLine("Alarm " + String(i + 1) + ": " + String(alarmHours[i]) + ":" + String(alarmMinutes[i]) + ":" + String(alarmSeconds[i]), 1, i * 30, 0);
    } else {
      printLine("Alarm " + String(i + 1) + ": Disabled", 1, i * 30, 0);
    }
  }
  delay(5000);
}

void deleteAlarm() {
  display.clearDisplay();
  bool alarm1Set = alarmEnabled[0];
  bool alarm2Set = alarmEnabled[1];

  if (!alarm1Set && !alarm2Set) {
    printLine("No Alarms Set!", 1, 30, 0);
    delay(2000);
    return;
  }

  if (alarm1Set && alarm2Set) {
    printLine("Delete Alarm 1(Press UP)", 1, 0, 0);
    printLine("Delete Alarm 2(Press DOWN)", 1, 20, 0);
    printLine("Delete Both(Press OK)", 1, 40, 0);
  } else if (alarm1Set) {
    printLine("Delete Alarm 1(Press UP)?", 1, 0, 0);
  } else if (alarm2Set) {
    printLine("Delete Alarm 2(Press DOWN)?", 1, 0, 0);
  }

  int pressed = waitForButtonPress();
  if (pressed == BUTTON_UP && alarm1Set) {
    alarmEnabled[0] = false;
    alarmTriggered[0] = false;
    printLine("Alarm 1 Deleted", 1, 50, 0);
  } else if (pressed == BUTTON_DOWN && alarm2Set) {
    alarmEnabled[1] = false;
    alarmTriggered[1] = false;
    printLine("Alarm 2 Deleted", 1, 50, 0);
  } else if (pressed == BUTTON_OK && alarm1Set && alarm2Set) {
    alarmEnabled[0] = false;
    alarmTriggered[0] = false;
    alarmEnabled[1] = false;
    alarmTriggered[1] = false;
    printLine("Both Alarms Deleted", 1, 50, 0);
  } else {
    printLine("Invalid Option", 1, 20, 0);
  }

  delay(2000);
}

void checkTempAndHumidity() {
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  display.clearDisplay();
  printLine("Temp: " + String(temperature) + "C", 1, 0, 0);
  printLine("Humidity: " + String(humidity) + "%", 1, 20, 0);

  if ((temperature < 24 || temperature > 32) && (humidity < 65 || humidity > 80)) {
    digitalWrite(LED_PIN_TH, HIGH);
    tone(BUZZER_PIN, 2000); 
    printLine("Hum & Temp Out of Range!", 1, 30, 0);
  } 
  else if (temperature < 24 || temperature > 32) {
    digitalWrite(LED_PIN_TH, HIGH);
    tone(BUZZER_PIN, 2000); 
    printLine("Temp Out of Range!", 1, 40, 0);
  } 
  else if (humidity < 65 || humidity > 80) {
    digitalWrite(LED_PIN_TH, HIGH);
    tone(BUZZER_PIN, 2000); 
    printLine("Hum Out of Range!", 1, 40, 0);
  } 
  else {
    digitalWrite(LED_PIN_TH, LOW);
    noTone(BUZZER_PIN);
  }
}    

void handleLightMonitoring() {
  unsigned long current_time = millis();
  
  if (current_time - last_sample_time >= sampling_interval * 1000) {
    float reading = analogRead(LDR_PIN) / 4095.0;
    light_readings[reading_index] = reading;
    reading_index = (reading_index + 1) % (int)(sending_interval / sampling_interval);
    
    if (!initialized && reading_index == 0) {
      initialized = true;
    }
    last_sample_time = current_time;
  }
  
  if (current_time - last_send_time >= sending_interval * 1000) {
    float avg_light = calculate_average_light();
    float temperature = dht.readTemperature();
    
    if (!isnan(temperature)) {
      float motor_angle = calculate_motor_angle(avg_light, temperature);
      servo.write(motor_angle);
        // Debug prints before publishing
      Serial.println("[DEBUG] Publishing sensor data:");
      Serial.print("  - Light Intensity: "); Serial.println(avg_light, 3); // 3 decimal places
      Serial.print("  - Temperature: "); Serial.print(temperature, 1); Serial.println("°C");
      Serial.print("  - Motor Angle: "); Serial.print(motor_angle, 1); Serial.println("°");
      
      char light_msg[50];
      char temp_msg[50];
      char angle_msg[50];
      
      dtostrf(avg_light, 4, 3, light_msg);
      dtostrf(temperature, 4, 2, temp_msg);
      dtostrf(motor_angle, 4, 2, angle_msg);
      
      client.publish(light_intensity_topic, light_msg);
      client.publish(temperature_topic, temp_msg);
      client.publish(motor_angle_topic, angle_msg);
      
      display.clearDisplay();
      printLine("Light: " + String(avg_light), 1, 0, 0);
      printLine("Temp: " + String(temperature) + "C", 1, 10, 0);
      printLine("Angle: " + String(motor_angle) + "°", 1, 20, 0);
      display.display();
    }
    
    last_send_time = current_time;
  }
}

float calculate_average_light() {
  int num_readings = initialized ? (int)(sending_interval / sampling_interval) : reading_index;
  if (num_readings == 0) return 0;
  
  float sum = 0;
  for (int i = 0; i < num_readings; i++) {
    sum += light_readings[i];
  }
  return sum / num_readings;
}

float calculate_motor_angle(float light_intensity, float temperature) {
  if (sampling_interval <= 0 || sending_interval <= 0 || T_med <= 0) {
    return theta_offset;
  }
   // Clamp light intensity to valid range [0,1]
  light_intensity = constrain(light_intensity, 0.0, 1.0);
  
  // Handle case where sampling and sending intervals are equal (log(1) = 0)
 // float interval_ratio = sampling_interval / sending_interval;
  //float log_value = (fabs(interval_ratio - 1.0) < 0.001) ? 0.0 : log(interval_ratio);
  
  // Protect against extreme temperature values
  temperature = constrain(temperature, 10.0, 40.0); // DHT11 range
  
  // Calculate angle with intermediate bounds checking
  float intensity_factor = light_intensity * gamma_val;
  float temp_factor = temperature / T_med;
   // 3. Calculate log term and FLIP ITS SIGN
  //float log_term = log(sampling_interval / sending_interval);
  //log_term = -log_term;  // Critical change: Multiply by -1

   // 3. Calculate interval ratio and log term (without sign flip)
  float interval_ratio = sampling_interval / sending_interval;
  float log_term = 0.0;
  
  // Only calculate log if ratio is significantly different from 1
  if (fabs(interval_ratio - 1.0) > 0.001) {
    log_term = log(interval_ratio);
  }

  // 4. Calculate motor angle using original equation
  float angle = theta_offset + (180 - theta_offset) * 
               light_intensity * gamma_val * 
               log_term * 
               (temperature / T_med);
  Serial.printf(
    "Calc: I=%.2f, γ=%.2f, ts/tu=%.2f, T/Tmed=%.2f → θ=%.1f\n",
    light_intensity, gamma_val, interval_ratio, temperature/T_med, angle
  );
  return constrain(angle, theta_offset, 180);
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  if (String(topic) == config_topic) {
    int start = message.indexOf("\"ts\":") + 5;
    int end = message.indexOf(",", start);
    if (end == -1) end = message.indexOf("}", start);
    sampling_interval = message.substring(start, end).toFloat();
    
    start = message.indexOf("\"tu\":") + 5;
    end = message.indexOf(",", start);
    if (end == -1) end = message.indexOf("}", start);
    sending_interval = message.substring(start, end).toFloat();
    
    start = message.indexOf("\"theta_offset\":") + 14;
    end = message.indexOf(",", start);
    if (end == -1) end = message.indexOf("}", start);
    theta_offset = message.substring(start, end).toFloat();
    
    start = message.indexOf("\"gamma\":") + 8;
    end = message.indexOf(",", start);
    if (end == -1) end = message.indexOf("}", start);
    gamma_val = message.substring(start, end).toFloat();  // Changed from gamma to gamma_val
    
    start = message.indexOf("\"T_med\":") + 8;
    end = message.indexOf("}", start);
    T_med = message.substring(start, end).toFloat();
    
    float current_light = calculate_average_light();
    float current_temp = dht.readTemperature();
    if (!isnan(current_temp)) {
      float new_angle = calculate_motor_angle(current_light, current_temp);
      servo.write(new_angle);
    }
  }
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "ESP32-wokwi";
    clientId += String(random(0xffff), HEX);
    
    if (client.connect(clientId.c_str())) {
      client.subscribe(config_topic);
      client.publish(status_topic, "Medibox connected");
    } else {
      delay(5000);
    }
  }
}