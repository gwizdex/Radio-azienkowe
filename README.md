# Radio-łazienkowe

Jesto to projekt radia do łazienki. <img width="101" height="69" alt="obraz" src="https://github.com/user-attachments/assets/56c87561-6b80-47ea-a032-c84eab18e1a0" />

Oparty jest o płytkę ESP32-S3 WROOM-1 N16R8 oraz MAX98357A i czujnik światła BH1750. Głośnik jaki został użyty to 4Ω głośnik do pracy w saunach.

Zasada działania: Czujnik oświetlenia wykrywa wartość powyżej ustawinego progu i rozpoczyna odtwarzania strumienia audio ze wcześnie zdefiniowanego adresu.
Mamy wstępnie wpisane cztery przykładowe stacje radiowe, można je dowolnie modyfikować.
Oprogramowanie daje możliwość ustawienie trybu pracy auto/manual. Wybór języka interfejsu, regulację głośności. 
Wszystkie wartości są zapamiętywane i obowiązują po restarcie.

Mamy również do dyspozycji tryb serwisowy: edycja GPIO, czujnik DHT22, konfiguracja MQTT (Home Assistant), zmiana sieci WiFi oraz **zarządzanie stacjami radiowymi** (dodawanie i usuwanie — do 20 pozycji). Strona główna służy do codziennego sterowania; ustawienia zaawansowane są w menu serwisowym.

Dostęp do menu serwisowego i powiązanych endpointów API jest zabezpieczony **Basic Auth**: użytkownik `admin`, hasło `jolka` (wartość domyślna w repozytorium — na własnym urządzeniu zmień w kodzie stałych `SERVICE_AUTH_USER` / `SERVICE_AUTH_PASS`). Hasło brokera MQTT **nie jest zwracane** do przeglądarki; przy zapisie puste pole hasła **nie nadpisuje** zapisanego wcześniej.

Przy pierwszym uruchomieniu uruchamia się tryb AP, szukamy sieci o nazwie: "Radio_Config" i hasło: "password123"
Po uzyskaniu połączenia otrzymamy komunikat głosowy o uzyskanym dresie IP w dwóch językach polskim i angielskim.
Stacje radiowe można znaleźć tu: https://fmstream.org/

### Zmiany (ostatnia aktualizacja firmware)

- **Aktualizacja OTA (ArduinoOTA)** — po pierwszym wgraniu przez USB kolejne firmware można wgrywać przez WiFi z Arduino IDE (port sieciowy `Radio-Lazienka`, hasło jak w panelu serwisowym).
- **Stała głośność komunikatów TTS** — komunikaty startowe (adres IP, błąd WiFi) odtwarzane są zawsze z głośności `TTS_VOLUME` (domyślnie 14/100), niezależnie od ustawionej głośności radia.
- **Basic Auth** — logowanie HTTP do `/service` i endpointów konfiguracyjnych zamiast hasła wpisywanego w JavaScript.
- **Bezpieczeństwo MQTT** — `/getmqtt` zwraca tylko flagę `hasPassword`, nie hasło w jawnej postaci.
- **Zarządzanie stacjami** — dodawanie i usuwanie stacji przeniesione z panelu głównego do menu serwisowego.
- **Walidacja po stronie urządzenia** — m.in. numery GPIO (0–48), port MQTT (1–65535), adres brokera (IPv4 lub nazwa DNS z kropką, np. `homeassistant.local`), progi i opóźnienia, godziny harmonogramu `HH:MM`, adresy strumieni `http://` / `https://`, głośność 0–100.
- **Płynniejszy interfejs WWW** — podczas TTS, zmiany stacji, odczytu BH1750 i konfiguracji WiFi używane są opóźnienia nieblokujące (`serviceDelay`), dzięki czemu strona i odtwarzanie reagują w trakcie tych operacji.

Ustawienia Arduino: 
Board: "ESP32S3 Dev Module"
USB CDC On Boot: "Enabled"
CPU Frequency: "240MHz (WiFi)"
Flash Mode: "QIO 80MHz"
Flash Size: "16MB (128Mb)"
Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"
PSRAM: "OPI PSRAM"
Upload Mode: "UART0 / Hardware CDC"
Upload Speed: "921600"
USB Mode: "Hardware CDC and JTAG"
Core Debug Level: "None"


This is a bathroom radio project.<img width="112" height="77" alt="obraz" src="https://github.com/user-attachments/assets/85d266f2-89bc-47e9-a971-574fa8c852a2" />


It's based on the ESP32-S3 WROOM-1 N16R8 board, the MAX98357A, and the BH1750 light sensor. The speaker used is a 4Ω speaker suitable for saunas.

Operation: The light sensor detects a value above a set threshold and starts playing an audio stream from a pre-defined address.
Four sample radio stations are pre-programmed, and these can be freely modified.
The software allows you to set auto/manual operation modes, select the interface language, and adjust the volume.
All values ​​are saved and remain valid after a reboot.

We also have a **service mode**: GPIO editing, DHT22 sensor, MQTT (Home Assistant), WiFi reconfiguration, and **radio station management** (add/remove — up to 20 stations). The main page is for everyday control; advanced settings are in the service menu.

Service mode and related API endpoints are protected with **Basic Auth**: username `admin`, password `jolka` (default in this repo — change `SERVICE_AUTH_USER` / `SERVICE_AUTH_PASS` in code on your own device). The MQTT broker password is **not sent** to the browser; leaving the password field empty when saving **does not overwrite** a previously stored password.

When you first turn it on, it launches AP mode, searches for a network named "Radio_Config" and enters the password: "password123."
Once connected, you'll receive a voice message announcing the IP address you've acquired, in both Polish and English.
Radio stations can be found here: https://fmstream.org/

### Changes (latest firmware update)

- **OTA updates (ArduinoOTA)** — after the first USB flash, upload new firmware over WiFi from the Arduino IDE (network port `Radio-Lazienka`, password same as the service panel).
- **Fixed TTS announcement volume** — startup voice messages (IP address, WiFi error) always play at `TTS_VOLUME` (default 14/100), independent of the radio volume setting.
- **Basic Auth** — HTTP login for `/service` and configuration endpoints instead of a JavaScript password prompt.
- **MQTT security** — `/getmqtt` returns only a `hasPassword` flag, not the plaintext password.
- **Station management** — add/remove stations moved from the main panel to the service menu.
- **Server-side validation** — GPIO numbers (0–48), MQTT port (1–65535), broker host (IPv4 or dotted DNS name, e.g. `homeassistant.local`), thresholds and delays, schedule times `HH:MM`, stream URLs `http://` / `https://`, volume 0–100.
- **More responsive web UI** — non-blocking delays (`serviceDelay`) during TTS, station changes, BH1750 reads, and WiFi setup so the page and playback stay responsive.

Arduino settings:
Board: "ESP32S3 Dev Module"
USB CDC On Boot: "Enabled"
CPU Frequency: "240MHz (WiFi)"
Flash Mode: "QIO 80MHz"
Flash Size: "16MB (128Mb)"
Partition Scheme: "16M Flash (3MB APP/9.9MB FATFS)"
PSRAM: "OPI PSRAM"
Upload Mode: "UART0 / Hardware CDC"
Upload Speed: "921600"
USB Mode: "Hardware CDC and JTAG"
Core Debug Level: "None"

<img width="364" height="768" alt="obraz" src="https://github.com/user-attachments/assets/1b45c4a7-7a9e-4599-965d-3c1414bb8b17" />
<img width="311" height="330" alt="obraz" src="https://github.com/user-attachments/assets/81fc124d-c8ea-4546-96f1-82c82b3038e2" />
<img width="339" height="482" alt="obraz" src="https://github.com/user-attachments/assets/202bf12d-0967-467e-8f24-0e9e915ae188" />
<img width="682" height="560" alt="obraz" src="https://github.com/user-attachments/assets/150a7881-c7fb-4bcb-83f1-071366da3fd1" />
<img width="2235" height="1111" alt="obraz" src="https://github.com/user-attachments/assets/2ad1d452-2292-4b51-93e3-e3e8a1839517" />

<img width="827" height="1235" alt="obraz" src="https://github.com/user-attachments/assets/cd736d84-6096-4187-96a0-e043e82894fe" />
<img width="829" height="1034" alt="obraz" src="https://github.com/user-attachments/assets/2e6fe498-be6a-48a3-bd96-93f422fcb6f6" />
<img width="837" height="666" alt="obraz" src="https://github.com/user-attachments/assets/84d8f45a-ab84-45e8-8cae-4e6bc50cc878" />
<img width="855" height="682" alt="obraz" src="https://github.com/user-attachments/assets/6e302d6d-9e50-4858-b4db-fa398aeaf7cf" />
<img width="828" height="987" alt="obraz" src="https://github.com/user-attachments/assets/6875e549-ed59-422d-b6a6-1d9eab9ef306" />
<img width="838" height="832" alt="obraz" src="https://github.com/user-attachments/assets/92744017-2229-4d89-9c5d-f3fff51ccf1d" />
<img width="827" height="1311" alt="obraz" src="https://github.com/user-attachments/assets/8b3cff2f-27d4-421e-8d70-e7de1060ec3a" />

