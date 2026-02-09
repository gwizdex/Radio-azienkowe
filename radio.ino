#include <WiFi.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <Preferences.h>
#include <Wire.h>
#include "Audio.h"
#include "esp_task_wdt.h"
#include <time.h>
#include <DHT.h>
#include <PubSubClient.h>

// Piny - konfigurowane przez użytkownika
int SDA_PIN = 8;
int SCL_PIN = 9;
int BH1750_ADDR = 0x23;

// Piny I2S dla ESP32-S3
int I2S_DOUT = 6;
int I2S_BCLK = 4;
int I2S_LRC = 5;

// DHT22
int DHT_PIN = 10;
bool DHT_ENABLED = false;
DHT *dht = nullptr;

// MQTT
String MQTT_SERVER = "";
int MQTT_PORT = 1883;
String MQTT_USER = "";
String MQTT_PASS = "";
bool MQTT_ENABLED = false;
String MQTT_TOPIC_PREFIX = "bathroom_radio";

WiFiClient espClient;
PubSubClient mqttClient(espClient);
unsigned long lastMQTTPublish = 0;

#define MAX_STATIONS 20

Audio audio;
WebServer server(80);
Preferences preferences;

float brightness = 0;
bool lightOn = false;
bool manualMode = false;
int currentVolume = 5;
String currentStation = "Antyradio";

// DHT22 dane
float temperature = 0.0;
float humidity = 0.0;
unsigned long lastDHTRead = 0;

// Zmienne konfiguracyjne
float brightnessThreshold = 10.0;
int delayOn = 1;
int delayOff = 1;

// Harmonogram czasowy
int scheduleStartHour = 6;
int scheduleStartMinute = 0;
int scheduleEndHour = 23;
int scheduleEndMinute = 59;

struct RadioStation {
  String name;
  String url;
};

RadioStation stations[MAX_STATIONS];
int stationCount = 0;

// ========================================
// FUNKCJE TTS (Text-to-Speech) - DWUJĘZYCZNE
// ========================================

String urlEncode(String str) {
  String encoded = "";
  char c;
  for (int i = 0; i < str.length(); i++) {
    c = str.charAt(i);
    if (c == ' ') {
      encoded += '+';
    } else if (isalnum(c)) {
      encoded += c;
    } else {
      encoded += '%';
      if (c < 16) encoded += '0';
      encoded += String(c, HEX);
    }
  }
  return encoded;
}

// Flaga blokująca loop podczas TTS
volatile bool isSpeaking = false;

void speak(String text, int volume = 21) {
  // Google TTS API URL
  String ttsURL = "http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=pl&q=" + 
                  urlEncode(text);
  
  Serial.println("Speaking: " + text);
  
  isSpeaking = true; // BLOKADA!
  
  int previousVolume = currentVolume;
  audio.setVolume(volume);
  audio.connecttohost(ttsURL.c_str());
  
  // Czekaj aż audio się zakończy
  unsigned long startTime = millis();
  unsigned long timeout = text.length() * 100 + 5000; // 100ms na znak + 5s marginesu
  
  while (audio.isRunning() && (millis() - startTime < timeout)) {
    audio.loop(); // Ważne! Musimy wywołać loop() tutaj
    delay(10);
  }
  
  audio.stopSong();
  delay(500); // Krótka przerwa
  
  audio.setVolume(previousVolume);
  
  isSpeaking = false; // ODBLOKOWANIE
  
  Serial.println("Speaking finished");
}

void speakIP(String ip) {
  // Zamień kropki na "kropka" dla lepszej wymowy
  String ipTextPL = ip;
  ipTextPL.replace(".", " kropka ");
  
  String ipTextEN = ip;
  ipTextEN.replace(".", " dot ");
  
  // POLSKI
  String messagePL = "Połączono. Adres I P: " + ipTextPL;
  speak(messagePL, 5);
  
  delay(1000); // Krótka przerwa między językami
  
  // ANGIELSKI
  String ttsURL = "http://translate.google.com/translate_tts?ie=UTF-8&client=tw-ob&tl=en&q=" + 
                  urlEncode("Connected. I P address: " + ipTextEN);
  
  Serial.println("Speaking (EN): Connected. IP address: " + ip);
  
  isSpeaking = true;
  
  int previousVolume = currentVolume;
  audio.setVolume(5);
  audio.connecttohost(ttsURL.c_str());
  
  unsigned long startTime = millis();
  unsigned long timeout = 10000; // 10s max
  
  while (audio.isRunning() && (millis() - startTime < timeout)) {
    audio.loop();
    delay(10);
  }
  
  audio.stopSong();
  delay(500);
  
  audio.setVolume(previousVolume);
  isSpeaking = false;
  
  Serial.println("Speaking finished (both languages)");
}

// ========================================
// FUNKCJE ŁADOWANIA/ZAPISYWANIA
// ========================================

void loadPins() {
  SDA_PIN = preferences.getInt("sda_pin", 8);
  SCL_PIN = preferences.getInt("scl_pin", 9);
  I2S_DOUT = preferences.getInt("i2s_dout", 6);
  I2S_BCLK = preferences.getInt("i2s_bclk", 4);
  I2S_LRC = preferences.getInt("i2s_lrc", 5);
  DHT_PIN = preferences.getInt("dht_pin", 10);
  DHT_ENABLED = preferences.getBool("dht_enabled", false);
}

void savePins() {
  preferences.putInt("sda_pin", SDA_PIN);
  preferences.putInt("scl_pin", SCL_PIN);
  preferences.putInt("i2s_dout", I2S_DOUT);
  preferences.putInt("i2s_bclk", I2S_BCLK);
  preferences.putInt("i2s_lrc", I2S_LRC);
  preferences.putInt("dht_pin", DHT_PIN);
  preferences.putBool("dht_enabled", DHT_ENABLED);
}

void loadMQTT() {
  MQTT_SERVER = preferences.getString("mqtt_server", "");
  MQTT_PORT = preferences.getInt("mqtt_port", 1883);
  MQTT_USER = preferences.getString("mqtt_user", "");
  MQTT_PASS = preferences.getString("mqtt_pass", "");
  MQTT_ENABLED = preferences.getBool("mqtt_enabled", false);
  MQTT_TOPIC_PREFIX = preferences.getString("mqtt_prefix", "bathroom_radio");
}

void saveMQTT() {
  preferences.putString("mqtt_server", MQTT_SERVER);
  preferences.putInt("mqtt_port", MQTT_PORT);
  preferences.putString("mqtt_user", MQTT_USER);
  preferences.putString("mqtt_pass", MQTT_PASS);
  preferences.putBool("mqtt_enabled", MQTT_ENABLED);
  preferences.putString("mqtt_prefix", MQTT_TOPIC_PREFIX);
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  Serial.println("MQTT received: " + String(topic) + " = " + message);
  
  String topicStr = String(topic);
  
  if (topicStr.endsWith("/cmd/play")) {
    manualMode = true;
    lightOn = true;
    audio.connecttohost(getStationURL(currentStation).c_str());
    audio.setVolume(currentVolume);
    Serial.println("MQTT: Play");
  }
  else if (topicStr.endsWith("/cmd/stop")) {
    manualMode = true;
    lightOn = false;
    audio.stopSong();
    Serial.println("MQTT: Stop");
  }
  else if (topicStr.endsWith("/cmd/volume")) {
    int vol = message.toInt();
    if (vol >= 0 && vol <= 100) {
      currentVolume = vol;
      preferences.putInt("volume", currentVolume);
      audio.setVolume(currentVolume);
      Serial.println("MQTT: Volume = " + String(vol));
    }
  }
  else if (topicStr.endsWith("/cmd/station")) {
    currentStation = message;
    preferences.putString("station", currentStation);
    if (lightOn) {
      audio.stopSong();
      delay(500);
      audio.connecttohost(getStationURL(currentStation).c_str());
      audio.setVolume(currentVolume);
    }
    Serial.println("MQTT: Station = " + message);
  }
  else if (topicStr.endsWith("/cmd/mode")) {
    if (message == "auto") {
      manualMode = false;
      Serial.println("MQTT: Mode = AUTO");
    } else if (message == "manual") {
      manualMode = true;
      Serial.println("MQTT: Mode = MANUAL");
    }
  }
}

void connectMQTT() {
  if (!MQTT_ENABLED || MQTT_SERVER.length() == 0) return;
  
  if (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    
    String clientId = "ESP32Radio-" + String(WiFi.macAddress());
    
    bool connected = false;
    if (MQTT_USER.length() > 0) {
      connected = mqttClient.connect(clientId.c_str(), MQTT_USER.c_str(), MQTT_PASS.c_str());
    } else {
      connected = mqttClient.connect(clientId.c_str());
    }
    
    if (connected) {
      Serial.println(" connected!");
      
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/cmd/play").c_str());
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/cmd/stop").c_str());
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/cmd/volume").c_str());
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/cmd/station").c_str());
      mqttClient.subscribe((MQTT_TOPIC_PREFIX + "/cmd/mode").c_str());
      
      Serial.println("MQTT subscribed to commands");
    } else {
      Serial.print(" failed, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void publishMQTT() {
  if (!MQTT_ENABLED || !mqttClient.connected()) return;
  
  if (millis() - lastMQTTPublish > 10000) {
    lastMQTTPublish = millis();
    
    mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/playing").c_str(), lightOn ? "ON" : "OFF", true);
    mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/mode").c_str(), manualMode ? "manual" : "auto", true);
    mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/station").c_str(), currentStation.c_str(), true);
    mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/volume").c_str(), String(currentVolume).c_str(), true);
    mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/brightness").c_str(), String(brightness).c_str(), true);
    
    if (DHT_ENABLED) {
      mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/temperature").c_str(), String(temperature).c_str(), true);
      mqttClient.publish((MQTT_TOPIC_PREFIX + "/state/humidity").c_str(), String(humidity).c_str(), true);
    }
    
    Serial.println("MQTT: Published state");
  }
}

void initDHT() {
  if (DHT_ENABLED) {
    if (dht != nullptr) {
      delete dht;
    }
    dht = new DHT(DHT_PIN, DHT22);
    dht->begin();
    Serial.println("DHT22 initialized on GPIO " + String(DHT_PIN));
  } else {
    if (dht != nullptr) {
      delete dht;
      dht = nullptr;
    }
    Serial.println("DHT22 disabled");
  }
}

void readDHT() {
  if (!DHT_ENABLED || dht == nullptr) return;
  
  if (millis() - lastDHTRead > 10000) {
    lastDHTRead = millis();
    
    float h = dht->readHumidity();
    float t = dht->readTemperature();
    
    if (!isnan(h) && !isnan(t)) {
      humidity = h;
      temperature = t;
      Serial.printf("DHT22: Temp=%.1f°C, Humidity=%.1f%%\n", temperature, humidity);
    } else {
      Serial.println("DHT22: Failed to read");
    }
  }
}

void loadStations() {
  stationCount = preferences.getInt("stationCount", 0);
  
  if (stationCount == 0) {
    stations[0] = {"Antyradio", "https://an03.cdn.eurozet.pl/ant-waw.mp3"};
    stations[1] = {"RMF FM", "http://rmfstream1.interia.pl:8000/rmf_fm"};
    stations[2] = {"Radio ZET", "http://zet-net-01.cdn.eurozet.pl:8400/"};
    stations[3] = {"Depeche Mode", "http://195.150.20.243/rmf_depeche_mode"};
    stationCount = 4;
    saveStations();
  } else {
    for (int i = 0; i < stationCount; i++) {
      String nameKey = "name_" + String(i);
      String urlKey = "url_" + String(i);
      stations[i].name = preferences.getString(nameKey.c_str(), "");
      stations[i].url = preferences.getString(urlKey.c_str(), "");
    }
  }
}

void saveStations() {
  preferences.putInt("stationCount", stationCount);
  for (int i = 0; i < stationCount; i++) {
    String nameKey = "name_" + String(i);
    String urlKey = "url_" + String(i);
    preferences.putString(nameKey.c_str(), stations[i].name);
    preferences.putString(urlKey.c_str(), stations[i].url);
  }
}

void loadSettings() {
  brightnessThreshold = preferences.getFloat("threshold", 10.0);
  delayOn = preferences.getInt("delayOn", 1);
  delayOff = preferences.getInt("delayOff", 1);
  scheduleStartHour = preferences.getInt("schedStartH", 6);
  scheduleStartMinute = preferences.getInt("schedStartM", 0);
  scheduleEndHour = preferences.getInt("schedEndH", 23);
  scheduleEndMinute = preferences.getInt("schedEndM", 59);
}

void saveSettings() {
  preferences.putFloat("threshold", brightnessThreshold);
  preferences.putInt("delayOn", delayOn);
  preferences.putInt("delayOff", delayOff);
  preferences.putInt("schedStartH", scheduleStartHour);
  preferences.putInt("schedStartM", scheduleStartMinute);
  preferences.putInt("schedEndH", scheduleEndHour);
  preferences.putInt("schedEndM", scheduleEndMinute);
}

bool isWithinSchedule() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time");
    return true;
  }
  
  int currentMinutes = timeinfo.tm_hour * 60 + timeinfo.tm_min;
  int startMinutes = scheduleStartHour * 60 + scheduleStartMinute;
  int endMinutes = scheduleEndHour * 60 + scheduleEndMinute;
  
  if (startMinutes <= endMinutes) {
    return (currentMinutes >= startMinutes && currentMinutes <= endMinutes);
  } else {
    return (currentMinutes >= startMinutes || currentMinutes <= endMinutes);
  }
}

float readBH1750() {
  Wire.beginTransmission(BH1750_ADDR);
  Wire.write(0x10);
  Wire.endTransmission();
  delay(120);
  
  Wire.requestFrom(BH1750_ADDR, 2);
  if (Wire.available() == 2) {
    uint16_t value = Wire.read() << 8 | Wire.read();
    return value / 1.2;
  }
  return 0;
}

String getStationURL(String name) {
  for (int i = 0; i < stationCount; i++) {
    if (stations[i].name == name) {
      return stations[i].url;
    }
  }
  return stations[0].url;
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Radio Łazienka</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #f0f0f0; }
    .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    .header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px; }
    h1 { margin: 0; color: #333; }
    .lang-switch { display: flex; gap: 5px; }
    .lang-btn { width: 32px; height: 32px; border: 2px solid #ddd; border-radius: 5px; cursor: pointer; background-size: cover; background-position: center; transition: all 0.3s; }
    .lang-btn:hover { transform: scale(1.1); border-color: #007bff; }
    .lang-btn.active { border-color: #007bff; box-shadow: 0 0 5px rgba(0,123,255,0.5); }
    .lang-pl { background-image: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 640 480"><rect width="640" height="480" fill="%23fff"/><rect width="640" height="240" y="240" fill="%23dc143c"/></svg>'); }
    .lang-en { background-image: url('data:image/svg+xml,<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 60 30"><clipPath id="a"><path d="M0 0v30h60V0z"/></clipPath><clipPath id="b"><path d="M30 15h30v15zv15H0zH0V0zV0h30z"/></clipPath><g clip-path="url(%23a)"><path d="M0 0v30h60V0z" fill="%23012169"/><path d="M0 0l60 30m0-30L0 30" stroke="%23fff" stroke-width="6"/><path d="M0 0l60 30m0-30L0 30" clip-path="url(%23b)" stroke="%23C8102E" stroke-width="4"/><path d="M30 0v30M0 15h60" stroke="%23fff" stroke-width="10"/><path d="M30 0v30M0 15h60" stroke="%23C8102E" stroke-width="6"/></g></svg>'); }
    h2 { color: #555; margin-top: 30px; border-bottom: 2px solid #007bff; padding-bottom: 5px; }
    .control { margin: 20px 0; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    select, input[type="range"], input[type="text"], input[type="number"], input[type="time"], input[type="password"] { width: 100%; padding: 10px; font-size: 16px; box-sizing: border-box; }
    .value { text-align: center; font-size: 24px; color: #007bff; margin: 10px 0; }
    .status { padding: 10px; background: #e7f3ff; border-left: 4px solid #007bff; margin: 10px 0; }
    .status.manual { background: #fff3cd; border-left-color: #ffc107; }
    .status.outside-schedule { background: #f8d7da; border-left-color: #dc3545; }
    button { width: 100%; padding: 15px; font-size: 16px; background: #007bff; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 10px; }
    button:hover { background: #0056b3; }
    button.play { background: #28a745; }
    button.play:hover { background: #218838; }
    button.stop { background: #dc3545; }
    button.stop:hover { background: #c82333; }
    button.delete { background: #dc3545; }
    button.delete:hover { background: #c82333; }
    button.save { background: #28a745; }
    button.save:hover { background: #218838; }
    button.service { background: #6c757d; font-size: 14px; }
    button.service:hover { background: #5a6268; }
    .station-item { background: #f8f9fa; padding: 10px; margin: 10px 0; border-radius: 5px; display: flex; justify-content: space-between; align-items: center; }
    .station-info { flex-grow: 1; }
    .station-name { font-weight: bold; color: #333; }
    .station-url { font-size: 12px; color: #666; word-break: break-all; }
    .btn-small { padding: 5px 15px; font-size: 14px; width: auto; margin: 0 5px; }
    .setting-group { background: #f8f9fa; padding: 15px; border-radius: 5px; margin: 10px 0; }
    .inline-control { display: flex; align-items: center; gap: 10px; margin: 10px 0; }
    .inline-control label { margin: 0; flex: 1; }
    .inline-control input { flex: 0 0 100px; }
    .time-range { display: grid; grid-template-columns: 1fr auto 1fr; gap: 10px; align-items: center; margin: 10px 0; }
    .time-range input { flex: 1; }
    .control-buttons { display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 15px 0; }
    .control-buttons button { margin: 0; }
    .mode-switch { display: flex; gap: 10px; margin: 15px 0; }
    .mode-btn { flex: 1; padding: 15px; font-size: 16px; border: 2px solid #ddd; background: white; color: #666; border-radius: 5px; cursor: pointer; transition: all 0.3s; }
    .mode-btn.active { background: #007bff; color: white; border-color: #007bff; }
    .mode-btn:hover { transform: scale(1.02); }
    .footer { margin-top: 30px; padding-top: 20px; border-top: 1px solid #ddd; text-align: center; }
    .dht-display { display: none; }
    .dht-display.visible { display: block; }
  </style>
</head>
<body>
  <div class="container">
    <div class="header">
      <h1>🎵 <span data-lang-key="title">Radio Łazienka</span></h1>
      <div class="lang-switch">
        <div class="lang-btn lang-pl active" onclick="changeLang('pl')" title="Polski"></div>
        <div class="lang-btn lang-en" onclick="changeLang('en')" title="English"></div>
      </div>
    </div>
    
    <div class="status" id="statusBox">
      <strong><span data-lang-key="brightness">Jasność</span>:</strong> <span id="brightness">-</span> lx<br>
      <div class="dht-display" id="dhtDisplay">
        <strong><span data-lang-key="temperature">Temperatura</span>:</strong> <span id="temperature">-</span>°C<br>
        <strong><span data-lang-key="humidity">Wilgotność</span>:</strong> <span id="humidity">-</span>%<br>
      </div>
      <strong><span data-lang-key="status">Status</span>:</strong> <span id="status">-</span><br>
      <strong><span data-lang-key="schedule">Harmonogram</span>:</strong> <span id="scheduleStatus">-</span>
    </div>

    <div class="mode-switch">
      <button class="mode-btn active" id="autoBtn" onclick="setMode('auto')">🤖 <span data-lang-key="mode-auto">Automatyczny</span></button>
      <button class="mode-btn" id="manualBtn" onclick="setMode('manual')">✋ <span data-lang-key="mode-manual">Ręczny</span></button>
    </div>

    <div class="control-buttons">
      <button class="play" onclick="manualPlay()">▶️ <span data-lang-key="play">Odtwarzaj</span></button>
      <button class="stop" onclick="manualStop()">⏸️ <span data-lang-key="stop">Zatrzymaj</span></button>
    </div>
    
    <div class="control">
      <label><span data-lang-key="station-select">Wybór stacji</span>:</label>
      <select id="station" onchange="changeStation()"></select>
    </div>
    
    <div class="control">
      <label><span data-lang-key="volume">Głośność</span>:</label>
      <input type="range" id="volume" min="0" max="100" step="1" value="5" oninput="updateVolume(this.value)">
      <div class="value"><span id="volumeValue">5</span>%</div>
    </div>

    <h2>🕐 <span data-lang-key="schedule-title">Harmonogram</span></h2>
    <div class="setting-group">
      <label><span data-lang-key="active-period">Przedział aktywności</span>:</label>
      <div class="time-range">
        <input type="time" id="scheduleStart" value="06:00">
        <span data-lang-key="to">do</span>
        <input type="time" id="scheduleEnd" value="23:59">
      </div>
      <button class="save" onclick="saveSettings()">💾 <span data-lang-key="save-schedule">Zapisz harmonogram</span></button>
    </div>

    <h2>⚙️ <span data-lang-key="sensor-settings">Ustawienia czujnika</span></h2>
    <div class="setting-group">
      <div class="inline-control">
        <label><span data-lang-key="threshold">Próg jasności</span> (lx):</label>
        <input type="number" id="threshold" min="1" max="1000" step="0.1" value="10">
      </div>
      <div class="inline-control">
        <label><span data-lang-key="delay-on">Opóźnienie włączenia</span> (s):</label>
        <input type="number" id="delayOn" min="0" max="60" step="1" value="1">
      </div>
      <div class="inline-control">
        <label><span data-lang-key="delay-off">Opóźnienie wyłączenia</span> (s):</label>
        <input type="number" id="delayOff" min="0" max="60" step="1" value="1">
      </div>
      <button class="save" onclick="saveSettings()">💾 <span data-lang-key="save-settings">Zapisz ustawienia</span></button>
    </div>

    <h2>📻 <span data-lang-key="manage-stations">Zarządzaj stacjami</span></h2>
    <div id="stationList"></div>

    <h2>➕ <span data-lang-key="add-station">Dodaj nową stację</span> <span data-lang-key="max-stations">(max 20 stacji)</span></h2>
    <div class="control">
      <label><span data-lang-key="station-name">Nazwa stacji</span>:</label>
      <input type="text" id="newStationName" placeholder="np. Radio Nowa">
    </div>
    <div class="control">
      <label><span data-lang-key="stream-url">URL strumienia</span>:</label>
      <input type="text" id="newStationUrl" placeholder="http://...">
    </div>
    <button onclick="addStation()">➕ <span data-lang-key="add-btn">Dodaj stację</span></button>

    <div class="footer">
      <button class="service" onclick="openServiceMenu()">🔧 <span data-lang-key="service-menu">Menu serwisowe</span></button>
    </div>
  </div>

  <script>
    var currentLang = 'pl';
    var translations = {
      pl: {
        title: 'Radio Łazienka',
        brightness: 'Jasność',
        temperature: 'Temperatura',
        humidity: 'Wilgotność',
        status: 'Status',
        schedule: 'Harmonogram',
        play: 'Odtwarzaj',
        stop: 'Zatrzymaj',
        'station-select': 'Wybór stacji',
        volume: 'Głośność',
        'schedule-title': 'Harmonogram',
        'active-period': 'Przedział aktywności',
        to: 'do',
        'save-schedule': 'Zapisz harmonogram',
        'sensor-settings': 'Ustawienia czujnika',
        threshold: 'Próg jasności',
        'delay-on': 'Opóźnienie włączenia',
        'delay-off': 'Opóźnienie wyłączenia',
        'save-settings': 'Zapisz ustawienia',
        'manage-stations': 'Zarządzaj stacjami',
        'add-station': 'Dodaj nową stację',
        'max-stations': '(max 20 stacji)',
        'station-name': 'Nazwa stacji',
        'stream-url': 'URL strumienia',
        'add-btn': 'Dodaj stację',
        'mode-auto': 'Automatyczny',
        'mode-manual': 'Ręczny',
        'schedule-active': 'Aktywny',
        'schedule-inactive': 'Nieaktywny',
        'status-playing': '▶️ Gra',
        'status-stopped': '⏸️ Stop',
        delete: 'Usuń',
        'service-menu': 'Menu serwisowe'
      },
      en: {
        title: 'Bathroom Radio',
        brightness: 'Brightness',
        temperature: 'Temperature',
        humidity: 'Humidity',
        status: 'Status',
        schedule: 'Schedule',
        play: 'Play',
        stop: 'Stop',
        'station-select': 'Select station',
        volume: 'Volume',
        'schedule-title': 'Schedule',
        'active-period': 'Active period',
        to: 'to',
        'save-schedule': 'Save schedule',
        'sensor-settings': 'Sensor settings',
        threshold: 'Brightness threshold',
        'delay-on': 'Turn on delay',
        'delay-off': 'Turn off delay',
        'save-settings': 'Save settings',
        'manage-stations': 'Manage stations',
        'add-station': 'Add new station',
        'max-stations': '(max 20 stations)',
        'station-name': 'Station name',
        'stream-url': 'Stream URL',
        'add-btn': 'Add station',
        'mode-auto': 'Automatic',
        'mode-manual': 'Manual',
        'schedule-active': 'Active',
        'schedule-inactive': 'Inactive',
        'status-playing': '▶️ Playing',
        'status-stopped': '⏸️ Stopped',
        delete: 'Delete',
        'service-menu': 'Service menu'
      }
    };

    var changeLang = function(lang) {
      currentLang = lang;
      localStorage.setItem('lang', lang);
      
      document.querySelectorAll('.lang-btn').forEach(function(btn) {
        btn.classList.remove('active');
      });
      document.querySelector('.lang-' + lang).classList.add('active');
      
      document.querySelectorAll('[data-lang-key]').forEach(function(elem) {
        var key = elem.getAttribute('data-lang-key');
        if (translations[lang][key]) {
          elem.textContent = translations[lang][key];
        }
      });
      
      if(lang === 'pl') {
        document.getElementById('newStationName').placeholder = 'np. Radio Nowa';
        document.getElementById('newStationUrl').placeholder = 'http://...';
      } else {
        document.getElementById('newStationName').placeholder = 'e.g. New Radio';
        document.getElementById('newStationUrl').placeholder = 'http://...';
      }
      
      updateData();
      updateStationList();
    };

    var setMode = function(mode) {
      if(mode === 'auto') {
        document.getElementById('autoBtn').classList.add('active');
        document.getElementById('manualBtn').classList.remove('active');
        fetch('/setmode?mode=auto');
      } else {
        document.getElementById('autoBtn').classList.remove('active');
        document.getElementById('manualBtn').classList.add('active');
        fetch('/setmode?mode=manual');
      }
    };

    var openServiceMenu = function() {
      var password = prompt(currentLang === 'pl' ? 'Podaj hasło serwisowe:' : 'Enter service password:');
      if (password === 'jolka') {
        window.location.href = '/service';
      } else if (password !== null) {
        alert(currentLang === 'pl' ? 'Nieprawidłowe hasło!' : 'Incorrect password!');
      }
    };

    var stationsData = [];

    var updateStationList = function() {
      fetch('/stations').then(function(r){return r.json();}).then(function(d){
        stationsData = d.stations;
        var sel = document.getElementById('station');
        sel.innerHTML = '';
        for(var i=0; i<stationsData.length; i++){
          var opt = document.createElement('option');
          opt.value = stationsData[i].name;
          opt.textContent = stationsData[i].name;
          sel.appendChild(opt);
        }
        var list = document.getElementById('stationList');
        list.innerHTML = '';
        var deleteText = translations[currentLang]['delete'];
        for(var i=0; i<stationsData.length; i++){
          var item = document.createElement('div');
          item.className = 'station-item';
          item.innerHTML = '<div class="station-info"><div class="station-name">'+stationsData[i].name+'</div><div class="station-url">'+stationsData[i].url+'</div></div><button class="btn-small delete" onclick="deleteStation('+i+')">🗑️ '+deleteText+'</button>';
          list.appendChild(item);
        }
      });
    };

    var updateData = function() {
      fetch('/data').then(function(r){return r.json();}).then(function(d){
        document.getElementById('brightness').textContent = d.brightness.toFixed(1);
        
        if(d.dhtEnabled) {
          document.getElementById('dhtDisplay').classList.add('visible');
          document.getElementById('temperature').textContent = d.temperature.toFixed(1);
          document.getElementById('humidity').textContent = d.humidity.toFixed(1);
        } else {
          document.getElementById('dhtDisplay').classList.remove('visible');
        }
        
        var statusText = d.playing ? translations[currentLang]['status-playing'] : translations[currentLang]['status-stopped'];
        document.getElementById('status').textContent = statusText;
        
        document.getElementById('station').value = d.station;
        document.getElementById('volume').value = d.volume;
        document.getElementById('volumeValue').textContent = d.volume;
        
        if(d.manual) {
          document.getElementById('autoBtn').classList.remove('active');
          document.getElementById('manualBtn').classList.add('active');
        } else {
          document.getElementById('autoBtn').classList.add('active');
          document.getElementById('manualBtn').classList.remove('active');
        }
        
        var schedText = d.inSchedule ? translations[currentLang]['schedule-active'] : translations[currentLang]['schedule-inactive'];
        document.getElementById('scheduleStatus').textContent = schedText;
        
        var statusBox = document.getElementById('statusBox');
        if(d.manual) {
          statusBox.className = 'status manual';
        } else if(!d.inSchedule) {
          statusBox.className = 'status outside-schedule';
        } else {
          statusBox.className = 'status';
        }
      });
    };

    var loadSettings = function() {
      fetch('/settings').then(function(r){return r.json();}).then(function(d){
        document.getElementById('threshold').value = d.threshold;
        document.getElementById('delayOn').value = d.delayOn;
        document.getElementById('delayOff').value = d.delayOff;
        document.getElementById('scheduleStart').value = d.scheduleStart;
        document.getElementById('scheduleEnd').value = d.scheduleEnd;
      });
    };
    
    var changeStation = function() {
      var s = document.getElementById('station').value;
      fetch('/station?name=' + encodeURIComponent(s));
    };
    
    var updateVolume = function(val) {
      document.getElementById('volumeValue').textContent = val;
      fetch('/volume?val=' + val);
    };

    var manualPlay = function() {
      fetch('/play').then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){updateData();}
      });
    };

    var manualStop = function() {
      fetch('/stop').then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){updateData();}
      });
    };

    var saveSettings = function() {
      var t = document.getElementById('threshold').value;
      var on = document.getElementById('delayOn').value;
      var off = document.getElementById('delayOff').value;
      var start = document.getElementById('scheduleStart').value;
      var end = document.getElementById('scheduleEnd').value;
      fetch('/savesettings?threshold='+t+'&delayOn='+on+'&delayOff='+off+'&schedStart='+encodeURIComponent(start)+'&schedEnd='+encodeURIComponent(end))
      .then(function(r){return r.text();}).then(function(res){
        var msg = res==='OK' ? (currentLang==='pl' ? 'Ustawienia zapisane!' : 'Settings saved!') : (currentLang==='pl' ? 'Błąd!' : 'Error!');
        alert(msg);
      });
    };

    var addStation = function() {
      var n = document.getElementById('newStationName').value.trim();
      var u = document.getElementById('newStationUrl').value.trim();
      if(!n || !u){ 
        alert(currentLang==='pl' ? 'Wypełnij wszystkie pola!' : 'Fill in all fields!'); 
        return; 
      }
      fetch('/addstation', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'name='+encodeURIComponent(n)+'&url='+encodeURIComponent(u)})
      .then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){
          document.getElementById('newStationName').value = '';
          document.getElementById('newStationUrl').value = '';
          updateStationList();
          alert(currentLang==='pl' ? 'Stacja dodana!' : 'Station added!');
        }else{
          alert(currentLang==='pl' ? 'Błąd: '+res : 'Error: '+res);
        }
      });
    };

    var deleteStation = function(idx) {
      var confirmMsg = currentLang==='pl' ? 'Czy na pewno usunąć tę stację?' : 'Are you sure you want to delete this station?';
      if(!confirm(confirmMsg)) return;
      fetch('/deletestation?index='+idx).then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){
          updateStationList();
          alert(currentLang==='pl' ? 'Stacja usunięta!' : 'Station deleted!');
        }else{
          alert(currentLang==='pl' ? 'Błąd: '+res : 'Error: '+res);
        }
      });
    };
    
    var savedLang = localStorage.getItem('lang') || 'pl';
    changeLang(savedLang);
    
    setInterval(updateData, 2000);
    updateData();
    updateStationList();
    loadSettings();
  </script>
</body>
</html>
)rawliteral";
const char service_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>Menu Serwisowe</title>
  <style>
    body { font-family: Arial; margin: 20px; background: #f0f0f0; }
    .container { max-width: 500px; margin: auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
    h1 { text-align: center; color: #dc3545; }
    h2 { color: #555; margin-top: 30px; border-bottom: 2px solid #dc3545; padding-bottom: 5px; }
    .control { margin: 20px 0; }
    label { display: block; margin-bottom: 5px; font-weight: bold; }
    input[type="number"], input[type="text"], select { width: 100%; padding: 10px; font-size: 16px; box-sizing: border-box; }
    button { width: 100%; padding: 15px; font-size: 16px; background: #28a745; color: white; border: none; border-radius: 5px; cursor: pointer; margin-top: 10px; }
    button:hover { background: #218838; }
    button.back { background: #6c757d; }
    button.back:hover { background: #5a6268; }
    button.restart { background: #ffc107; color: #000; }
    button.restart:hover { background: #e0a800; }
    button.reset { background: #dc3545; }
    button.reset:hover { background: #c82333; }
    button.wifi { background: #17a2b8; }
    button.wifi:hover { background: #138496; }
    .warning { background: #fff3cd; border-left: 4px solid #ffc107; padding: 15px; margin: 20px 0; }
    .danger { background: #f8d7da; border-left: 4px solid #dc3545; padding: 15px; margin: 20px 0; }
    .info { background: #d1ecf1; border-left: 4px solid #17a2b8; padding: 15px; margin: 20px 0; }
    .setting-group { background: #f8f9fa; padding: 15px; border-radius: 5px; margin: 10px 0; }
    .inline-control { display: grid; grid-template-columns: 1fr auto; gap: 10px; align-items: center; margin: 10px 0; }
    .inline-control label { margin: 0; }
    .inline-control select, .inline-control input[type="number"] { width: 100px; }
    .toggle-switch { position: relative; display: inline-block; width: 50px; height: 24px; }
    .toggle-switch input { opacity: 0; width: 0; height: 0; }
    .slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 24px; }
    .slider:before { position: absolute; content: ""; height: 18px; width: 18px; left: 3px; bottom: 3px; background-color: white; transition: .4s; border-radius: 50%; }
    input:checked + .slider { background-color: #28a745; }
    input:checked + .slider:before { transform: translateX(26px); }
  </style>
</head>
<body>
  <div class="container">
    <h1>🔧 <span data-lang="title">Menu Serwisowe</span></h1>
    
    <div class="warning">
      <strong>⚠️ <span data-lang="warning-title">Uwaga!</span></strong><br>
      <span data-lang="warning-text">Zmiana pinów wymaga restartu urządzenia. Upewnij się, że podajesz prawidłowe numery GPIO.</span>
    </div>

    <h2><span data-lang="wifi-title">Konfiguracja WiFi</span></h2>
    <div class="info">
      <strong>📡 <span data-lang="wifi-current">Aktualna sieć</span>:</strong> <span id="currentSSID">-</span><br>
      <strong>📶 <span data-lang="wifi-signal">Siła sygnału</span>:</strong> <span id="wifiRSSI">-</span> dBm
    </div>
    <button class="wifi" onclick="changeWiFi()">📶 <span data-lang="change-wifi">Zmień sieć WiFi</span></button>

    <h2><span data-lang="mqtt-title">MQTT - Home Assistant</span></h2>
    <div class="setting-group">
      <div class="inline-control">
        <label><span data-lang="mqtt-enable">Włącz MQTT</span>:</label>
        <label class="toggle-switch">
          <input type="checkbox" id="mqtt_enabled" onchange="toggleMQTT()">
          <span class="slider"></span>
        </label>
      </div>
      <div class="control">
        <label><span data-lang="mqtt-server">Serwer MQTT (IP)</span>:</label>
        <input type="text" id="mqtt_server" placeholder="192.168.1.100">
      </div>
      <div class="control">
        <label><span data-lang="mqtt-port">Port</span>:</label>
        <input type="number" id="mqtt_port" min="1" max="65535" value="1883">
      </div>
      <div class="control">
        <label><span data-lang="mqtt-user">Użytkownik (opcjonalnie)</span>:</label>
        <input type="text" id="mqtt_user" placeholder="">
      </div>
      <div class="control">
        <label><span data-lang="mqtt-pass">Hasło (opcjonalnie)</span>:</label>
        <input type="text" id="mqtt_pass" placeholder="">
      </div>
      <div class="control">
        <label><span data-lang="mqtt-prefix">Prefix topików</span>:</label>
        <input type="text" id="mqtt_prefix" value="bathroom_radio">
      </div>
      <button onclick="saveMQTT()">💾 <span data-lang="save-mqtt">Zapisz MQTT</span></button>
    </div>

    <h2><span data-lang="dht-title">DHT22 - Temperatura i wilgotność</span></h2>
    <div class="setting-group">
      <div class="inline-control">
        <label><span data-lang="dht-enable">Włącz czujnik DHT22</span>:</label>
        <label class="toggle-switch">
          <input type="checkbox" id="dht_enabled" onchange="toggleDHT()">
          <span class="slider"></span>
        </label>
      </div>
      <div class="control">
        <label><span data-lang="dht-pin">DHT22 Pin (GPIO)</span>:</label>
        <input type="number" id="dht_pin" min="0" max="48" value="10">
      </div>
    </div>

    <h2><span data-lang="i2c-title">I2C - Czujnik światła BH1750</span></h2>
    <div class="setting-group">
      <div class="control">
        <label><span data-lang="sda-pin">SDA Pin (GPIO)</span>:</label>
        <input type="number" id="sda_pin" min="0" max="48" value="8">
      </div>
      <div class="control">
        <label><span data-lang="scl-pin">SCL Pin (GPIO)</span>:</label>
        <input type="number" id="scl_pin" min="0" max="48" value="9">
      </div>
    </div>

    <h2><span data-lang="i2s-title">I2S - Audio (MAX98357A)</span></h2>
    <div class="setting-group">
      <div class="control">
        <label><span data-lang="dout-pin">DOUT Pin (DIN) (GPIO)</span>:</label>
        <input type="number" id="i2s_dout" min="0" max="48" value="6">
      </div>
      <div class="control">
        <label><span data-lang="bclk-pin">BCLK Pin (SCK) (GPIO)</span>:</label>
        <input type="number" id="i2s_bclk" min="0" max="48" value="4">
      </div>
      <div class="control">
        <label><span data-lang="lrc-pin">LRC Pin (WS) (GPIO)</span>:</label>
        <input type="number" id="i2s_lrc" min="0" max="48" value="5">
      </div>
    </div>

    <button onclick="savePins()">💾 <span data-lang="save-pins">Zapisz ustawienia pinów</span></button>

    <h2><span data-lang="reset-title">Reset urządzenia</span></h2>
    <div class="danger">
      <strong>🚨 <span data-lang="danger-title">Niebezpieczeństwo!</span></strong><br>
      <span data-lang="danger-text">Kasowanie przywróci wszystkie ustawienia do wartości fabrycznych. Zostaną usunięte: wszystkie stacje radiowe, harmonogramy, ustawienia czujnika oraz konfiguracja pinów. Ta operacja jest nieodwracalna!</span>
    </div>
    
    <button class="reset" onclick="resetSettings()">🗑️ <span data-lang="reset-all">Przywróć ustawienia fabryczne</span></button>
    <button class="restart" onclick="restartDevice()">🔄 <span data-lang="restart">Restart urządzenia</span></button>
    <button class="back" onclick="goBack()">◀️ <span data-lang="back">Powrót</span></button>
  </div>

  <script>
    var currentLang = localStorage.getItem('lang') || 'pl';
    
    var translations = {
      pl: {
        title: 'Menu Serwisowe',
        'warning-title': 'Uwaga!',
        'warning-text': 'Zmiana pinów wymaga restartu urządzenia. Upewnij się, że podajesz prawidłowe numery GPIO.',
        'wifi-title': 'Konfiguracja WiFi',
        'wifi-current': 'Aktualna sieć',
        'wifi-signal': 'Siła sygnału',
        'change-wifi': 'Zmień sieć WiFi',
        'mqtt-title': 'MQTT - Home Assistant',
        'mqtt-enable': 'Włącz MQTT',
        'mqtt-server': 'Serwer MQTT (IP)',
        'mqtt-port': 'Port',
        'mqtt-user': 'Użytkownik (opcjonalnie)',
        'mqtt-pass': 'Hasło (opcjonalnie)',
        'mqtt-prefix': 'Prefix topików',
        'save-mqtt': 'Zapisz MQTT',
        'dht-title': 'DHT22 - Temperatura i wilgotność',
        'dht-enable': 'Włącz czujnik DHT22',
        'dht-pin': 'DHT22 Pin (GPIO)',
        'i2c-title': 'I2C - Czujnik światła BH1750',
        'sda-pin': 'SDA Pin (GPIO)',
        'scl-pin': 'SCL Pin (GPIO)',
        'i2s-title': 'I2S - Audio (MAX98357A)',
        'dout-pin': 'DOUT Pin (DIN) (GPIO)',
        'bclk-pin': 'BCLK Pin (SCK) (GPIO)',
        'lrc-pin': 'LRC Pin (WS) (GPIO)',
        'save-pins': 'Zapisz ustawienia pinów',
        'reset-title': 'Reset urządzenia',
        'danger-title': 'Niebezpieczeństwo!',
        'danger-text': 'Kasowanie przywróci wszystkie ustawienia do wartości fabrycznych. Zostaną usunięte: wszystkie stacje radiowe, harmonogramy, ustawienia czujnika oraz konfiguracja pinów. Ta operacja jest nieodwracalna!',
        'reset-all': 'Przywróć ustawienia fabryczne',
        restart: 'Restart urządzenia',
        back: 'Powrót',
        'saved-msg': 'Ustawienia zapisane! Uruchom restart, aby zmiany weszły w życie.',
        'mqtt-saved': 'MQTT zapisane!',
        'error-msg': 'Błąd zapisu!',
        'restart-confirm': 'Czy na pewno chcesz zrestartować urządzenie?',
        'restart-msg': 'Urządzenie się restartuje. Poczekaj 30 sekund i odśwież stronę.',
        'reset-confirm': 'CZY NA PEWNO CHCESZ SKASOWAĆ WSZYSTKIE USTAWIENIA?\n\nZostaną usunięte:\n- Wszystkie stacje radiowe\n- Harmonogramy\n- Ustawienia czujnika\n- Konfiguracja pinów\n\nTej operacji NIE MOŻNA cofnąć!\n\nWpisz "TAK" aby potwierdzić:',
        'reset-success': 'Ustawienia zostały skasowane! Urządzenie się restartuje...',
        'reset-cancelled': 'Operacja anulowana.',
        'wifi-change-msg': 'Urządzenie uruchomi WiFi Manager.\n\nPołącz się z siecią:\n"Radio_Config"\nHasło: "password123"\n\nNastępnie wybierz nową sieć WiFi.\n\nKontynuować?'
      },
      en: {
        title: 'Service Menu',
        'warning-title': 'Warning!',
        'warning-text': 'Changing pins requires a device restart. Make sure you provide correct GPIO numbers.',
        'wifi-title': 'WiFi Configuration',
        'wifi-current': 'Current network',
        'wifi-signal': 'Signal strength',
        'change-wifi': 'Change WiFi network',
        'mqtt-title': 'MQTT - Home Assistant',
        'mqtt-enable': 'Enable MQTT',
        'mqtt-server': 'MQTT Server (IP)',
        'mqtt-port': 'Port',
        'mqtt-user': 'Username (optional)',
        'mqtt-pass': 'Password (optional)',
        'mqtt-prefix': 'Topic prefix',
        'save-mqtt': 'Save MQTT',
        'dht-title': 'DHT22 - Temperature & Humidity',
        'dht-enable': 'Enable DHT22 sensor',
        'dht-pin': 'DHT22 Pin (GPIO)',
        'i2c-title': 'I2C - Light Sensor BH1750',
        'sda-pin': 'SDA Pin (GPIO)',
        'scl-pin': 'SCL Pin (GPIO)',
        'i2s-title': 'I2S - Audio (MAX98357A)',
        'dout-pin': 'DOUT Pin (DIN) (GPIO)',
        'bclk-pin': 'BCLK Pin (SCK) (GPIO)',
        'lrc-pin': 'LRC Pin (WS) (GPIO)',
        'save-pins': 'Save pin settings',
        'reset-title': 'Device reset',
        'danger-title': 'Danger!',
        'danger-text': 'Reset will restore all settings to factory defaults. The following will be deleted: all radio stations, schedules, sensor settings, and pin configuration. This operation is irreversible!',
        'reset-all': 'Restore factory settings',
        restart: 'Restart device',
        back: 'Back',
        'saved-msg': 'Settings saved! Restart the device for changes to take effect.',
        'mqtt-saved': 'MQTT saved!',
        'error-msg': 'Save error!',
        'restart-confirm': 'Are you sure you want to restart the device?',
        'restart-msg': 'Device is restarting. Wait 30 seconds and refresh the page.',
        'reset-confirm': 'ARE YOU SURE YOU WANT TO DELETE ALL SETTINGS?\n\nThe following will be deleted:\n- All radio stations\n- Schedules\n- Sensor settings\n- Pin configuration\n\nThis operation CANNOT be undone!\n\nType "YES" to confirm:',
        'reset-success': 'Settings have been reset! Device is restarting...',
        'reset-cancelled': 'Operation cancelled.',
        'wifi-change-msg': 'Device will start WiFi Manager.\n\nConnect to network:\n"Radio_Config"\nPassword: "password123"\n\nThen select new WiFi network.\n\nContinue?'
      }
    };

    function applyLanguage() {
      document.querySelectorAll('[data-lang]').forEach(function(elem) {
        var key = elem.getAttribute('data-lang');
        if (translations[currentLang][key]) {
          elem.textContent = translations[currentLang][key];
        }
      });
    }

    function loadWiFiInfo() {
      fetch('/wifiinfo').then(function(r){return r.json();}).then(function(d){
        document.getElementById('currentSSID').textContent = d.ssid || '-';
        document.getElementById('wifiRSSI').textContent = d.rssi || '-';
      });
    }

    function loadPins() {
      fetch('/getpins').then(function(r){return r.json();}).then(function(d){
        document.getElementById('sda_pin').value = d.sda;
        document.getElementById('scl_pin').value = d.scl;
        document.getElementById('i2s_dout').value = d.dout;
        document.getElementById('i2s_bclk').value = d.bclk;
        document.getElementById('i2s_lrc').value = d.lrc;
        document.getElementById('dht_pin').value = d.dht_pin;
        document.getElementById('dht_enabled').checked = d.dht_enabled;
      });
    }

    function loadMQTT() {
      fetch('/getmqtt').then(function(r){return r.json();}).then(function(d){
        document.getElementById('mqtt_server').value = d.server || '';
        document.getElementById('mqtt_port').value = d.port || 1883;
        document.getElementById('mqtt_user').value = d.user || '';
        document.getElementById('mqtt_pass').value = d.pass || '';
        document.getElementById('mqtt_prefix').value = d.prefix || 'bathroom_radio';
        document.getElementById('mqtt_enabled').checked = d.enabled;
      });
    }

    function toggleDHT() {
      savePins();
    }

    function toggleMQTT() {
      saveMQTT();
    }

    function savePins() {
      var sda = document.getElementById('sda_pin').value;
      var scl = document.getElementById('scl_pin').value;
      var dout = document.getElementById('i2s_dout').value;
      var bclk = document.getElementById('i2s_bclk').value;
      var lrc = document.getElementById('i2s_lrc').value;
      var dht_pin = document.getElementById('dht_pin').value;
      var dht_en = document.getElementById('dht_enabled').checked ? '1' : '0';
      
      fetch('/savepins?sda='+sda+'&scl='+scl+'&dout='+dout+'&bclk='+bclk+'&lrc='+lrc+'&dht_pin='+dht_pin+'&dht_en='+dht_en)
      .then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){
          alert(translations[currentLang]['saved-msg']);
        }else{
          alert(translations[currentLang]['error-msg']);
        }
      });
    }

    function saveMQTT() {
      var server = document.getElementById('mqtt_server').value;
      var port = document.getElementById('mqtt_port').value;
      var user = document.getElementById('mqtt_user').value;
      var pass = document.getElementById('mqtt_pass').value;
      var prefix = document.getElementById('mqtt_prefix').value;
      var enabled = document.getElementById('mqtt_enabled').checked ? '1' : '0';
      
      fetch('/savemqtt?server='+encodeURIComponent(server)+'&port='+port+'&user='+encodeURIComponent(user)+'&pass='+encodeURIComponent(pass)+'&prefix='+encodeURIComponent(prefix)+'&enabled='+enabled)
      .then(function(r){return r.text();}).then(function(res){
        if(res==='OK'){
          alert(translations[currentLang]['mqtt-saved']);
        }else{
          alert(translations[currentLang]['error-msg']);
        }
      });
    }

    function changeWiFi() {
      if(confirm(translations[currentLang]['wifi-change-msg'])) {
        fetch('/changewifi').then(function(){
          alert(currentLang==='pl' ? 'WiFi Manager uruchomiony! Połącz się z "Radio_Config".' : 'WiFi Manager started! Connect to "Radio_Config".');
        });
      }
    }

    function resetSettings() {
      var confirmWord = currentLang === 'pl' ? 'TAK' : 'YES';
      var userInput = prompt(translations[currentLang]['reset-confirm']);
      
      if(userInput === confirmWord) {
        fetch('/reset').then(function(r){return r.text();}).then(function(res){
          if(res==='OK'){
            alert(translations[currentLang]['reset-success']);
            setTimeout(function(){ window.location.href = '/'; }, 2000);
          }
        });
      } else if(userInput !== null) {
        alert(translations[currentLang]['reset-cancelled']);
      }
    }

    function restartDevice() {
      if(confirm(translations[currentLang]['restart-confirm'])) {
        fetch('/restart').then(function(){
          alert(translations[currentLang]['restart-msg']);
        });
      }
    }

    function goBack() {
      window.location.href = '/';
    }

    applyLanguage();
    loadPins();
    loadMQTT();
    loadWiFiInfo();
    setInterval(loadWiFiInfo, 5000);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send_P(200, "text/html", index_html);
}

void handleService() {
  server.send_P(200, "text/html", service_html);
}

void handleGetPins() {
  String json = "{";
  json += "\"sda\":" + String(SDA_PIN) + ",";
  json += "\"scl\":" + String(SCL_PIN) + ",";
  json += "\"dout\":" + String(I2S_DOUT) + ",";
  json += "\"bclk\":" + String(I2S_BCLK) + ",";
  json += "\"lrc\":" + String(I2S_LRC) + ",";
  json += "\"dht_pin\":" + String(DHT_PIN) + ",";
  json += "\"dht_enabled\":" + String(DHT_ENABLED ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleGetMQTT() {
  String json = "{";
  json += "\"server\":\"" + MQTT_SERVER + "\",";
  json += "\"port\":" + String(MQTT_PORT) + ",";
  json += "\"user\":\"" + MQTT_USER + "\",";
  json += "\"pass\":\"" + MQTT_PASS + "\",";
  json += "\"prefix\":\"" + MQTT_TOPIC_PREFIX + "\",";
  json += "\"enabled\":" + String(MQTT_ENABLED ? "true" : "false");
  json += "}";
  server.send(200, "application/json", json);
}

void handleWiFiInfo() {
  String json = "{";
  json += "\"ssid\":\"" + WiFi.SSID() + "\",";
  json += "\"rssi\":" + String(WiFi.RSSI());
  json += "}";
  server.send(200, "application/json", json);
}

void handleChangeWiFi() {
  server.send(200, "text/plain", "Starting WiFi Manager...");
  
  delay(1000);
  
  audio.stopSong();
  server.stop();
  
  WiFi.disconnect(true);
  delay(1000);
  
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(300);
  
  if (!wifiManager.startConfigPortal("Radio_Config", "password123")) {
    Serial.println("Failed to connect, restarting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("WiFi reconfigured! Restarting...");
  delay(1000);
  ESP.restart();
}

void handleSavePins() {
  if (server.hasArg("sda")) SDA_PIN = server.arg("sda").toInt();
  if (server.hasArg("scl")) SCL_PIN = server.arg("scl").toInt();
  if (server.hasArg("dout")) I2S_DOUT = server.arg("dout").toInt();
  if (server.hasArg("bclk")) I2S_BCLK = server.arg("bclk").toInt();
  if (server.hasArg("lrc")) I2S_LRC = server.arg("lrc").toInt();
  if (server.hasArg("dht_pin")) DHT_PIN = server.arg("dht_pin").toInt();
  if (server.hasArg("dht_en")) {
    DHT_ENABLED = (server.arg("dht_en") == "1");
    initDHT();
  }
  
  savePins();
  server.send(200, "text/plain", "OK");
  Serial.println("Pins saved. Restart required.");
}

void handleSaveMQTT() {
  if (server.hasArg("server")) MQTT_SERVER = server.arg("server");
  if (server.hasArg("port")) MQTT_PORT = server.arg("port").toInt();
  if (server.hasArg("user")) MQTT_USER = server.arg("user");
  if (server.hasArg("pass")) MQTT_PASS = server.arg("pass");
  if (server.hasArg("prefix")) MQTT_TOPIC_PREFIX = server.arg("prefix");
  if (server.hasArg("enabled")) {
    MQTT_ENABLED = (server.arg("enabled") == "1");
    if (MQTT_ENABLED && MQTT_SERVER.length() > 0) {
      mqttClient.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
      mqttClient.setCallback(mqttCallback);
      connectMQTT();
    }
  }
  
  saveMQTT();
  server.send(200, "text/plain", "OK");
  Serial.println("MQTT saved.");
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    String mode = server.arg("mode");
    if (mode == "auto") {
      manualMode = false;
      Serial.println("Mode: AUTO");
    } else if (mode == "manual") {
      manualMode = true;
      Serial.println("Mode: MANUAL");
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleReset() {
  preferences.clear();
  
  SDA_PIN = 8;
  SCL_PIN = 9;
  I2S_DOUT = 6;
  I2S_BCLK = 4;
  I2S_LRC = 5;
  DHT_PIN = 10;
  DHT_ENABLED = false;
  MQTT_ENABLED = false;
  MQTT_SERVER = "";
  MQTT_PORT = 1883;
  MQTT_USER = "";
  MQTT_PASS = "";
  MQTT_TOPIC_PREFIX = "bathroom_radio";
  brightnessThreshold = 10.0;
  delayOn = 1;
  delayOff = 1;
  scheduleStartHour = 6;
  scheduleStartMinute = 0;
  scheduleEndHour = 23;
  scheduleEndMinute = 59;
  currentVolume = 5;
  currentStation = "Antyradio";
  
  savePins();
  saveMQTT();
  saveSettings();
  preferences.putInt("volume", currentVolume);
  preferences.putString("station", currentStation);
  
  stationCount = 0;
  stations[0] = {"Antyradio", "https://an03.cdn.eurozet.pl/ant-waw.mp3"};
  stations[1] = {"RMF FM", "http://rmfstream1.interia.pl:8000/rmf_fm"};
  stations[2] = {"Radio ZET", "http://zet-net-01.cdn.eurozet.pl:8400/"};
  stations[3] = {"Depeche Mode", "http://195.150.20.243/rmf_depeche_mode"};
  stationCount = 4;
  saveStations();
  
  server.send(200, "text/plain", "OK");
  Serial.println("Factory reset completed. Restarting...");
  delay(2000);
  ESP.restart();
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void handleData() {
  String json = "{";
  json += "\"brightness\":" + String(brightness) + ",";
  json += "\"playing\":" + String(lightOn ? "true" : "false") + ",";
  json += "\"manual\":" + String(manualMode ? "true" : "false") + ",";
  json += "\"inSchedule\":" + String(isWithinSchedule() ? "true" : "false") + ",";
  json += "\"station\":\"" + currentStation + "\",";
  json += "\"volume\":" + String(currentVolume) + ",";
  json += "\"dhtEnabled\":" + String(DHT_ENABLED ? "true" : "false") + ",";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSettings() {
  char startTime[6], endTime[6];
  sprintf(startTime, "%02d:%02d", scheduleStartHour, scheduleStartMinute);
  sprintf(endTime, "%02d:%02d", scheduleEndHour, scheduleEndMinute);
  
  String json = "{";
  json += "\"threshold\":" + String(brightnessThreshold) + ",";
  json += "\"delayOn\":" + String(delayOn) + ",";
  json += "\"delayOff\":" + String(delayOff) + ",";
  json += "\"scheduleStart\":\"" + String(startTime) + "\",";
  json += "\"scheduleEnd\":\"" + String(endTime) + "\"";
  json += "}";
  server.send(200, "application/json", json);
}

void handleSaveSettings() {
  if (server.hasArg("threshold")) {
    brightnessThreshold = server.arg("threshold").toFloat();
  }
  if (server.hasArg("delayOn")) {
    delayOn = server.arg("delayOn").toInt();
  }
  if (server.hasArg("delayOff")) {
    delayOff = server.arg("delayOff").toInt();
  }
  if (server.hasArg("schedStart")) {
    String start = server.arg("schedStart");
    sscanf(start.c_str(), "%d:%d", &scheduleStartHour, &scheduleStartMinute);
  }
  if (server.hasArg("schedEnd")) {
    String end = server.arg("schedEnd");
    sscanf(end.c_str(), "%d:%d", &scheduleEndHour, &scheduleEndMinute);
  }
  
  saveSettings();
  server.send(200, "text/plain", "OK");
  Serial.println("Settings saved");
}

void handlePlay() {
  if (!manualMode) return;
  
  lightOn = true;
  audio.connecttohost(getStationURL(currentStation).c_str());
  audio.setVolume(currentVolume);
  server.send(200, "text/plain", "OK");
  Serial.println("Manual PLAY - Playing: " + currentStation);
}

void handleStop() {
  if (!manualMode) return;
  
  lightOn = false;
  audio.stopSong();
  server.send(200, "text/plain", "OK");
  Serial.println("Manual STOP");
}

void handleStations() {
  String json = "{\"stations\":[";
  for (int i = 0; i < stationCount; i++) {
    if (i > 0) json += ",";
    json += "{\"name\":\"" + stations[i].name + "\",";
    json += "\"url\":\"" + stations[i].url + "\"}";
  }
  json += "]}";
  server.send(200, "application/json", json);
}

void handleAddStation() {
  if (stationCount >= MAX_STATIONS) {
    server.send(200, "text/plain", "MAX");
    return;
  }
  
  if (server.hasArg("name") && server.hasArg("url")) {
    String name = server.arg("name");
    String url = server.arg("url");
    
    stations[stationCount].name = name;
    stations[stationCount].url = url;
    stationCount++;
    
    saveStations();
    server.send(200, "text/plain", "OK");
    Serial.println("Added station: " + name);
  } else {
    server.send(400, "text/plain", "ERROR");
  }
}

void handleDeleteStation() {
  if (server.hasArg("index")) {
    int index = server.arg("index").toInt();
    
    if (index >= 0 && index < stationCount) {
      for (int i = index; i < stationCount - 1; i++) {
        stations[i] = stations[i + 1];
      }
      stationCount--;
      
      saveStations();
      server.send(200, "text/plain", "OK");
      Serial.println("Deleted station at index: " + String(index));
    } else {
      server.send(400, "text/plain", "INVALID_INDEX");
    }
  } else {
    server.send(400, "text/plain", "ERROR");
  }
}

void handleStation() {
  if (server.hasArg("name")) {
    currentStation = server.arg("name");
    preferences.putString("station", currentStation);
    
    if (lightOn) {
      audio.stopSong();
      delay(500);
      audio.connecttohost(getStationURL(currentStation).c_str());
      audio.setVolume(currentVolume);
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleVolume() {
  if (server.hasArg("val")) {
    currentVolume = server.arg("val").toInt();
    preferences.putInt("volume", currentVolume);
    audio.setVolume(currentVolume);
  }
  server.send(200, "text/plain", "OK");
}
void setup() {
  Serial.begin(115200);
  delay(2000);
  
  esp_task_wdt_deinit();
  
  Serial.println("\n\n=== ESP32-S3 Radio Starting ===");
  
  preferences.begin("radio", false);
  loadPins();
  loadMQTT();
  
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("I2C initialized on SDA:" + String(SDA_PIN) + " SCL:" + String(SCL_PIN));
  
  initDHT();
  
  currentVolume = preferences.getInt("volume", 5);
  currentStation = preferences.getString("station", "Antyradio");
  
  loadStations();
  loadSettings();
  
  Serial.println("Loaded " + String(stationCount) + " stations");
  
  // Inicjalizacja audio PRZED WiFi Manager (potrzebne do TTS)
  Serial.println("Initializing audio I2S...");
  Serial.println("Pins: BCLK=" + String(I2S_BCLK) + ", LRC=" + String(I2S_LRC) + ", DOUT=" + String(I2S_DOUT));
  
  audio.setPinout(I2S_BCLK, I2S_LRC, I2S_DOUT);
  delay(500);
  audio.setVolume(currentVolume);
  Serial.println("Audio initialized!");
  
  WiFiManager wifiManager;
  wifiManager.setConfigPortalTimeout(180);
  
  if (!wifiManager.autoConnect("Radio_Config", "password123")) {
    Serial.println("Failed to connect, restarting...");
    
    // 🎤 Komunikat głosowy o błędzie
    speak("Nie udało się połączyć z siecią. Urządzenie się restartuje.", 21);
    
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("Connected to WiFi!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("RSSI: ");
  Serial.println(WiFi.RSSI());
  
  // 🎤🎤🎤 KOMUNIKAT GŁOSOWY Z ADRESEM IP! 🎤🎤🎤
  String ipAddress = WiFi.localIP().toString();
  speakIP(ipAddress);
  
  // MQTT Setup
  if (MQTT_ENABLED && MQTT_SERVER.length() > 0) {
    mqttClient.setServer(MQTT_SERVER.c_str(), MQTT_PORT);
    mqttClient.setCallback(mqttCallback);
    connectMQTT();
  }
  
  configTime(3600, 3600, "pool.ntp.org", "time.nist.gov");
  Serial.println("NTP configured");
  delay(2000);
  
  server.on("/", handleRoot);
  server.on("/service", handleService);
  server.on("/getpins", handleGetPins);
  server.on("/getmqtt", handleGetMQTT);
  server.on("/wifiinfo", handleWiFiInfo);
  server.on("/changewifi", handleChangeWiFi);
  server.on("/savepins", handleSavePins);
  server.on("/savemqtt", handleSaveMQTT);
  server.on("/setmode", handleSetMode);
  server.on("/reset", handleReset);
  server.on("/restart", handleRestart);
  server.on("/data", handleData);
  server.on("/settings", handleSettings);
  server.on("/savesettings", handleSaveSettings);
  server.on("/play", handlePlay);
  server.on("/stop", handleStop);
  server.on("/stations", handleStations);
  server.on("/addstation", HTTP_POST, handleAddStation);
  server.on("/deletestation", handleDeleteStation);
  server.on("/station", handleStation);
  server.on("/volume", handleVolume);
  
  server.begin();
  
  Serial.println("=== READY ===");
  Serial.print("Web interface: http://");
  Serial.println(WiFi.localIP());
}

void loop() {
  server.handleClient();
  
  // BLOKADA podczas mówienia TTS!
  if (isSpeaking) {
    delay(10);
    return;
  }
  
  audio.loop();
  
  // MQTT
  if (MQTT_ENABLED) {
    if (!mqttClient.connected()) {
      connectMQTT();
    }
    mqttClient.loop();
    publishMQTT();
  }
  
  readDHT();
  
  if (manualMode) {
    delay(1);
    return;
  }
  
  if (!isWithinSchedule() && lightOn) {
    lightOn = false;
    audio.stopSong();
    Serial.println("Outside schedule - Stopped");
  }
  
  if (!isWithinSchedule()) {
    delay(1);
    return;
  }
  
  static unsigned long lastRead = 0;
  static unsigned long lightOnTime = 0;
  static unsigned long lightOffTime = 0;
  static bool waitingToTurnOn = false;
  static bool waitingToTurnOff = false;
  
  if (millis() - lastRead > 1000) {
    lastRead = millis();
    brightness = readBH1750();
    
    if (brightness > brightnessThreshold && !lightOn && !waitingToTurnOn) {
      waitingToTurnOn = true;
      lightOnTime = millis();
      waitingToTurnOff = false;
    } else if (brightness > brightnessThreshold && waitingToTurnOn) {
      if (millis() - lightOnTime >= delayOn * 1000) {
        lightOn = true;
        waitingToTurnOn = false;
        Serial.println("Auto Light ON - Playing: " + currentStation);
        audio.connecttohost(getStationURL(currentStation).c_str());
        audio.setVolume(currentVolume);
      }
    } else if (brightness <= brightnessThreshold && lightOn && !waitingToTurnOff) {
      waitingToTurnOff = true;
      lightOffTime = millis();
      waitingToTurnOn = false;
    } else if (brightness <= brightnessThreshold && waitingToTurnOff) {
      if (millis() - lightOffTime >= delayOff * 1000) {
        lightOn = false;
        waitingToTurnOff = false;
        Serial.println("Auto Light OFF - Stopped");
        audio.stopSong();
      }
    } else if (brightness <= brightnessThreshold && !lightOn) {
      waitingToTurnOn = false;
    } else if (brightness > brightnessThreshold && lightOn) {
      waitingToTurnOff = false;
    }
  }
  
  delay(1);
}

void audio_info(const char *info){
    Serial.print("audio: ");
    Serial.println(info);
}